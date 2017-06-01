// Copyright 2010-2014 Google
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

#include "ortools/algorithms/find_graph_symmetries.h"

#include <algorithm>
#include <limits>
#include <numeric>

#include "ortools/base/commandlineflags.h"
#include "ortools/base/stringprintf.h"
#include "ortools/base/join.h"
#include "ortools/algorithms/dense_doubly_linked_list.h"
#include "ortools/algorithms/dynamic_partition.h"
#include "ortools/algorithms/dynamic_permutation.h"
#include "ortools/algorithms/sparse_permutation.h"
#include "ortools/graph/util.h"
#include "ortools/util/iterators.h"

DEFINE_bool(minimize_permutation_support_size, false,
            "Tweak the algorithm to try and minimize the support size"
            " of the generators produced. This may negatively impact the"
            " performance, but works great on the sat_holeXXX benchmarks"
            " to reduce the support size.");

namespace operations_research {

namespace {
// Some routines used below.
void SwapFrontAndBack(std::vector<int>* v) {
  DCHECK(!v->empty());
  std::swap((*v)[0], v->back());
}

bool PartitionsAreCompatibleAfterPartIndex(const DynamicPartition& p1,
                                           const DynamicPartition& p2,
                                           int part_index) {
  const int num_parts = p1.NumParts();
  if (p2.NumParts() != num_parts) return false;
  for (int p = part_index; p < num_parts; ++p) {
    if (p1.SizeOfPart(p) != p2.SizeOfPart(p) ||
        p1.ParentOfPart(p) != p2.ParentOfPart(p)) {
      return false;
    }
  }
  return true;
}

// Whether the "l1" list maps to "l2" under the permutation "permutation".
// This method uses a transient bitmask on all the elements, which
// should be entirely false before the call (and will be restored as such
// after it).
//
// TODO(user): Make this method support multi-elements (i.e. an element may
// be repeated in the list), and see if that's sufficient to make the whole
// graph symmetry finder support multi-arcs.
template <class List>
bool ListMapsToList(const List& l1, const List& l2,
                    const DynamicPermutation& permutation,
                    std::vector<bool>* tmp_node_mask) {
  int num_elements_delta = 0;
  bool match = true;
  for (const int mapped_x : l2) {
    ++num_elements_delta;
    (*tmp_node_mask)[mapped_x] = true;
  }
  for (const int x : l1) {
    --num_elements_delta;
    const int mapped_x = permutation.ImageOf(x);
    if (!(*tmp_node_mask)[mapped_x]) {
      match = false;
      break;
    }
    (*tmp_node_mask)[mapped_x] = false;
  }
  if (num_elements_delta != 0) match = false;
  if (!match) {
    // We need to clean up tmp_node_mask.
    for (const int x : l2) (*tmp_node_mask)[x] = false;
  }
  return match;
}
}  // namespace

GraphSymmetryFinder::GraphSymmetryFinder(const Graph& graph, bool is_undirected)
    : graph_(graph),
      tmp_dynamic_permutation_(NumNodes()),
      tmp_node_mask_(NumNodes(), false),
      tmp_degree_(NumNodes(), 0),
      tmp_nodes_with_degree_(NumNodes() + 1) {
  // Set up an "unlimited" time limit by default.
  time_limit_ = TimeLimit::Infinite();
  tmp_partition_.Reset(NumNodes());
  if (is_undirected) {
    DCHECK(GraphIsSymmetric(graph));
  } else {
    // Compute the reverse adjacency lists.
    // First pass: compute the total in-degree of all nodes and put it in
    // reverse_adj_list_index (shifted by two; see below why).
    reverse_adj_list_index_.assign(graph.num_nodes() + /*shift*/ 2, 0);
    for (const int node : graph.AllNodes()) {
      for (const int arc : graph.OutgoingArcs(node)) {
        ++reverse_adj_list_index_[graph.Head(arc) + /*shift*/ 2];
      }
    }
    // Second pass: apply a cumulative sum over reverse_adj_list_index.
    // After that, reverse_adj_list contains:
    // [0, 0, in_degree(node0), in_degree(node0) + in_degree(node1), ...]
    std::partial_sum(reverse_adj_list_index_.begin() + /*shift*/ 2,
                     reverse_adj_list_index_.end(),
                     reverse_adj_list_index_.begin() + /*shift*/ 2);
    // Third pass: populate "flattened_reverse_adj_lists", using
    // reverse_adj_list_index[i] as a dynamic pointer to the yet-unpopulated
    // area of the reverse adjacency list of node #i.
    flattened_reverse_adj_lists_.assign(graph.num_arcs(), -1);
    for (const int node : graph.AllNodes()) {
      for (const int arc : graph.OutgoingArcs(node)) {
        flattened_reverse_adj_lists_
            [reverse_adj_list_index_[graph.Head(arc) + /*shift*/ 1]++] = node;
      }
    }
    // The last pass shifted reverse_adj_list_index, so it's now as we want it:
    // [0, in_degree(node0), in_degree(node0) + in_degree(node1), ...]
    if (DEBUG_MODE) {
      DCHECK_EQ(graph.num_arcs(), reverse_adj_list_index_[graph.num_nodes()]);
      for (const int i : flattened_reverse_adj_lists_) DCHECK_NE(i, -1);
    }
  }
}

bool GraphSymmetryFinder::IsGraphAutomorphism(
    const DynamicPermutation& permutation) const {
  for (const int base : permutation.AllMappingsSrc()) {
    const int image = permutation.ImageOf(base);
    if (image == base) continue;
    if (!ListMapsToList(graph_[base], graph_[image], permutation,
                        &tmp_node_mask_)) {
      return false;
    }
  }
  if (!reverse_adj_list_index_.empty()) {
    // The graph was not symmetric: we must also check the incoming arcs
    // to displaced nodes.
    for (const int base : permutation.AllMappingsSrc()) {
      const int image = permutation.ImageOf(base);
      if (image == base) continue;
      if (!ListMapsToList(TailsOfIncomingArcsTo(base),
                          TailsOfIncomingArcsTo(image), permutation,
                          &tmp_node_mask_)) {
        return false;
      }
    }
  }
  return true;
}

namespace {
// Specialized subroutine, to avoid code duplication: see its call site
// and its self-explanatory code.
template <class T>
inline void IncrementCounterForNonSingletons(const T& nodes,
                                             const DynamicPartition& partition,
                                             std::vector<int>* node_count,
                                             std::vector<int>* nodes_seen) {
  for (const int node : nodes) {
    if (partition.ElementsInSamePartAs(node).size() == 1) continue;
    const int count = ++(*node_count)[node];
    if (count == 1) nodes_seen->push_back(node);
  }
}
}  // namespace

void GraphSymmetryFinder::RecursivelyRefinePartitionByAdjacency(
    int first_unrefined_part_index, DynamicPartition* partition) {
  // Rename, for readability of the code below.
  std::vector<int>& tmp_nodes_with_nonzero_degree = tmp_stack_;

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
  std::vector<bool> adjacency_directions(1, /*outgoing*/ true);
  if (!reverse_adj_list_index_.empty()) {
    adjacency_directions.push_back(false);  // Also look at incoming arcs.
  }
  for (int part_index = first_unrefined_part_index;
       part_index < partition->NumParts();  // Moving target!
       ++part_index) {
    for (const bool outgoing_adjacency : adjacency_directions) {
      // Count the aggregated degree of all nodes, only looking at arcs that
      // come from/to the current part.
      if (outgoing_adjacency) {
        for (const int node : partition->ElementsInPart(part_index)) {
          IncrementCounterForNonSingletons(graph_[node], *partition,
                                           &tmp_degree_,
                                           &tmp_nodes_with_nonzero_degree);
        }
      } else {
        for (const int node : partition->ElementsInPart(part_index)) {
          IncrementCounterForNonSingletons(TailsOfIncomingArcsTo(node),
                                           *partition, &tmp_degree_,
                                           &tmp_nodes_with_nonzero_degree);
        }
      }
      // Group the nodes by (nonzero) degree. Remember the maximum degree.
      int max_degree = 0;
      for (const int node : tmp_nodes_with_nonzero_degree) {
        const int degree = tmp_degree_[node];
        tmp_degree_[node] = 0;  // To clean up after us.
        max_degree = std::max(max_degree, degree);
        tmp_nodes_with_degree_[degree].push_back(node);
      }
      tmp_nodes_with_nonzero_degree.clear();  // To clean up after us.
      // For each degree, refine the partition by the set of nodes with that
      // degree.
      for (int degree = 1; degree <= max_degree; ++degree) {
        partition->Refine(tmp_nodes_with_degree_[degree]);
        tmp_nodes_with_degree_[degree].clear();  // To clean up after us.
      }
    }
  }
}

void GraphSymmetryFinder::DistinguishNodeInPartition(
    int node, DynamicPartition* partition, std::vector<int>* new_singletons) {
  const int original_num_parts = partition->NumParts();
  partition->Refine(std::vector<int>(1, node));
  RecursivelyRefinePartitionByAdjacency(partition->PartOf(node), partition);

  // Explore the newly refined parts to gather all the new singletons.
  if (new_singletons != nullptr) {
    new_singletons->clear();
    for (int p = original_num_parts; p < partition->NumParts(); ++p) {
      const int parent = partition->ParentOfPart(p);
      // We may see the same singleton parent several times, so we guard them
      // with the tmp_node_mask_ boolean vector.
      if (!tmp_node_mask_[parent] && parent < original_num_parts &&
          partition->SizeOfPart(parent) == 1) {
        tmp_node_mask_[parent] = true;
        new_singletons->push_back(*partition->ElementsInPart(parent).begin());
      }
      if (partition->SizeOfPart(p) == 1) {
        new_singletons->push_back(*partition->ElementsInPart(p).begin());
      }
    }
    // Reset tmp_node_mask_.
    for (int p = original_num_parts; p < partition->NumParts(); ++p) {
      tmp_node_mask_[partition->ParentOfPart(p)] = false;
    }
  }
}

namespace {
void MergeNodeEquivalenceClassesAccordingToPermutation(
    const SparsePermutation& perm, MergingPartition* node_equivalence_classes,
    DenseDoublyLinkedList* sorted_representatives) {
  for (int c = 0; c < perm.NumCycles(); ++c) {
    // TODO(user): use the global element->image iterator when it exists.
    int prev = -1;
    for (const int e : perm.Cycle(c)) {
      if (prev >= 0) {
        const int removed_representative =
            node_equivalence_classes->MergePartsOf(prev, e);
        if (sorted_representatives != nullptr && removed_representative != -1) {
          sorted_representatives->Remove(removed_representative);
        }
      }
      prev = e;
    }
  }
}

// Subroutine used by FindSymmetries(); see its call site. This finds and
// outputs (in "pruned_other_nodes") the list of all representatives (under
// "node_equivalence_classes") that are in the same part as
// "representative_node" in "partition"; other than "representative_node"
// itself.
// "node_equivalence_classes" must be compatible with "partition", i.e. two
// nodes that are in the same equivalence class must also be in the same part.
//
// To do this in O(output size), we also need the
// "representatives_sorted_by_index_in_partition" data structure: the
// representatives of the nodes of the targeted part are contiguous in that
// linked list.
void GetAllOtherRepresentativesInSamePartAs(
    int representative_node, const DynamicPartition& partition,
    const DenseDoublyLinkedList& representatives_sorted_by_index_in_partition,
    MergingPartition* node_equivalence_classes,  // Only for debugging.
    std::vector<int>* pruned_other_nodes) {
  pruned_other_nodes->clear();
  const int part_index = partition.PartOf(representative_node);
  // Iterate on all contiguous representatives after the initial one...
  int repr = representative_node;
  while (true) {
    DCHECK_EQ(repr, node_equivalence_classes->GetRoot(repr));
    repr = representatives_sorted_by_index_in_partition.Prev(repr);
    if (repr < 0 || partition.PartOf(repr) != part_index) break;
    pruned_other_nodes->push_back(repr);
  }
  // ... and then on all contiguous representatives *before* it.
  repr = representative_node;
  while (true) {
    DCHECK_EQ(repr, node_equivalence_classes->GetRoot(repr));
    repr = representatives_sorted_by_index_in_partition.Next(repr);
    if (repr < 0 || partition.PartOf(repr) != part_index) break;
    pruned_other_nodes->push_back(repr);
  }

  // This code is a bit tricky, so we check that we're doing it right, by
  // comparing its output to the brute-force, O(Part size) version.
  // This also (partly) verifies that
  // "representatives_sorted_by_index_in_partition" is what it claims it is.
  if (DEBUG_MODE) {
    std::vector<int> expected_output;
    for (const int e : partition.ElementsInPart(part_index)) {
      if (node_equivalence_classes->GetRoot(e) != representative_node) {
        expected_output.push_back(e);
      }
    }
    node_equivalence_classes->KeepOnlyOneNodePerPart(&expected_output);
    for (int& x : expected_output) x = node_equivalence_classes->GetRoot(x);
    std::sort(expected_output.begin(), expected_output.end());
    std::vector<int> sorted_output = *pruned_other_nodes;
    std::sort(sorted_output.begin(), sorted_output.end());
    DCHECK_EQ(strings::Join(expected_output, " "),
              strings::Join(sorted_output, " "));
  }
}
}  // namespace

util::Status GraphSymmetryFinder::FindSymmetries(
    double time_limit_seconds, std::vector<int>* node_equivalence_classes_io,
    std::vector<std::unique_ptr<SparsePermutation>>* generators,
    std::vector<int>* factorized_automorphism_group_size) {
  // Initialization.
  time_limit_.reset(new TimeLimit(time_limit_seconds));
  IF_STATS_ENABLED(stats_.initialization_time.StartTimer());
  generators->clear();
  factorized_automorphism_group_size->clear();
  if (node_equivalence_classes_io->size() != NumNodes()) {
    return util::Status(util::error::INVALID_ARGUMENT,
                        "Invalid 'node_equivalence_classes_io'.");
  }
  DynamicPartition base_partition(*node_equivalence_classes_io);
  // Break all inherent asymmetries in the graph.
  {
    ScopedTimeDistributionUpdater u(&stats_.initialization_refine_time);
    RecursivelyRefinePartitionByAdjacency(/*first_unrefined_part_index=*/0,
                                          &base_partition);
  }
  if (time_limit_->LimitReached()) {
    return util::Status(util::error::DEADLINE_EXCEEDED,
                        "During the initial refinement.");
  }
  VLOG(4) << "Base partition: "
          << base_partition.DebugString(DynamicPartition::SORT_BY_PART);

  MergingPartition node_equivalence_classes(NumNodes());
  std::vector<std::vector<int>> permutations_displacing_node(NumNodes());
  std::vector<int> potential_root_image_nodes;
  IF_STATS_ENABLED(stats_.initialization_time.StopTimerAndAddElapsedTime());

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
  // base_partition with elements from non-singleton parts (the 'invariant
  // node'), until all parts are singletons.
  // We remember which nodes we picked as invariants, and also the successive
  // partition sizes as we refine it, to allow us to backtrack.
  // Then we'll perform 2) in the reverse order, backtracking the stack from 1)
  // as using another dedicated stack for the search (see below).
  IF_STATS_ENABLED(stats_.invariant_dive_time.StartTimer());
  struct InvariantDiveState {
    int invariant_node;
    int num_parts_before_refinement;

    InvariantDiveState(int node, int num_parts)
      : invariant_node(node), num_parts_before_refinement(num_parts) {}
  };
  std::vector<InvariantDiveState> invariant_dive_stack;
  // TODO(user): experiment with, and briefly describe the results of various
  // algorithms for picking the invariant node:
  // - random selection
  // - highest/lowest degree first
  // - enumerate by part index; or by part size
  // - etc.
  for (int invariant_node = 0; invariant_node < NumNodes(); ++invariant_node) {
    if (base_partition.ElementsInSamePartAs(invariant_node).size() == 1) {
      continue;
    }
    invariant_dive_stack.push_back(
        InvariantDiveState(invariant_node, base_partition.NumParts()));
    DistinguishNodeInPartition(invariant_node, &base_partition, nullptr);
    VLOG(4) << "Invariant dive: invariant node = " << invariant_node
            << "; partition after: "
            << base_partition.DebugString(DynamicPartition::SORT_BY_PART);
    if (time_limit_->LimitReached()) {
      return util::Status(util::error::DEADLINE_EXCEEDED,
                          "During the invariant dive.");
    }
  }
  DenseDoublyLinkedList representatives_sorted_by_index_in_partition(
      base_partition.ElementsInHierarchicalOrder());
  DynamicPartition image_partition = base_partition;
  IF_STATS_ENABLED(stats_.invariant_dive_time.StopTimerAndAddElapsedTime());
  // Now we've dived to the bottom: we're left with the identity permutation,
  // which we don't need as a generator. We move on to phase 2).

  IF_STATS_ENABLED(stats_.main_search_time.StartTimer());
  while (!invariant_dive_stack.empty()) {
    if (time_limit_->LimitReached()) break;
    // Backtrack the last step of 1) (the invariant dive).
    IF_STATS_ENABLED(stats_.invariant_unroll_time.StartTimer());
    const int root_node = invariant_dive_stack.back().invariant_node;
    const int base_num_parts =
        invariant_dive_stack.back().num_parts_before_refinement;
    invariant_dive_stack.pop_back();
    base_partition.UndoRefineUntilNumPartsEqual(base_num_parts);
    image_partition.UndoRefineUntilNumPartsEqual(base_num_parts);
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

    // The potential images of root_node are the nodes in its part. They can be
    // pruned by the already computed equivalence classes.
    // TODO(user): better elect the representative of each equivalence class
    // in order to reduce the permutation support down the line
    // TODO(user): Don't build a list; but instead use direct, inline iteration
    // on the representatives in the while() loop below, to benefit from the
    // incremental merging of the equivalence classes.
    DCHECK_EQ(1, node_equivalence_classes.NumNodesInSamePartAs(root_node));
    GetAllOtherRepresentativesInSamePartAs(
        root_node, base_partition, representatives_sorted_by_index_in_partition,
        &node_equivalence_classes, &potential_root_image_nodes);
    DCHECK(!potential_root_image_nodes.empty());
    IF_STATS_ENABLED(stats_.invariant_unroll_time.StopTimerAndAddElapsedTime());

    // Try to map "root_node" to all of its potential images. For each image,
    // we only care about finding a single compatible permutation, if it exists.
    while (!potential_root_image_nodes.empty()) {
      if (time_limit_->LimitReached()) break;
      VLOG(4) << "Potential (pruned) images of root node " << root_node
              << " left: [" << strings::Join(potential_root_image_nodes, " ")
              << "].";
      const int root_image_node = potential_root_image_nodes.back();
      VLOG(4) << "Trying image of root node: " << root_image_node;

      std::unique_ptr<SparsePermutation> permutation =
          FindOneSuitablePermutation(root_node, root_image_node,
                                     &base_partition, &image_partition,
                                     *generators, permutations_displacing_node);

      if (permutation != nullptr) {
        ScopedTimeDistributionUpdater u(&stats_.permutation_output_time);
        // We found a permutation. We store it in the list of generators, and
        // further prune out the remaining 'root' image candidates, taking into
        // account the permutation we just found.
        MergeNodeEquivalenceClassesAccordingToPermutation(
            *permutation, &node_equivalence_classes,
            &representatives_sorted_by_index_in_partition);
        // HACK(user): to make sure that we keep root_image_node as the
        // representant of its part, we temporarily move it to the front
        // of the vector, then move it again to the back so that it gets
        // deleted by the pop_back() below.
        SwapFrontAndBack(&potential_root_image_nodes);
        node_equivalence_classes.KeepOnlyOneNodePerPart(
            &potential_root_image_nodes);
        SwapFrontAndBack(&potential_root_image_nodes);

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
  IF_STATS_ENABLED(stats_.main_search_time.StopTimerAndAddElapsedTime());
  IF_STATS_ENABLED(LOG(INFO) << "Statistics: " << stats_.StatString());
  if (time_limit_->LimitReached()) {
    return util::Status(util::error::DEADLINE_EXCEEDED,
                        "Some automorphisms were found, but probably not all.");
  }
  return util::Status::OK;
}

namespace {
// This method can be easily understood in the context of
// ConfirmFullMatchOrFindNextMappingDecision(): see its call sites.
// Knowing that we want to map some element of part #part_index of
// "base_partition" to part #part_index of "image_partition", pick the "best"
// such mapping, for the global search algorithm.
inline void GetBestMapping(const DynamicPartition& base_partition,
                           const DynamicPartition& image_partition,
                           int part_index, int* base_node, int* image_node) {
  // As of pending CL 66620435, we've loosely tried three variants of
  // GetBestMapping():
  // 1) Just take the first element of the base part, map it to the first
  //    element of the image part.
  // 2) Just take the first element of the base part, and map it to itself if
  //    possible, else map it to the first element of the image part
  // 3) Scan all elements of the base parts until we find one that can map to
  //    itself. If there isn't one; we just fall back to the strategy 1).
  //
  // Variant 2) gives the best results on most benchmarks, in terms of speed,
  // but 3) yields much smaller supports for the sat_holeXXX benchmarks, as
  // long as it's combined with the other tweak enabled by
  // FLAGS_minimize_permutation_support_size.
  if (FLAGS_minimize_permutation_support_size) {
    // Variant 3).
    for (const int node : base_partition.ElementsInPart(part_index)) {
      if (image_partition.PartOf(node) == part_index) {
        *image_node = *base_node = node;
        return;
      }
    }
    *base_node = *base_partition.ElementsInPart(part_index).begin();
    *image_node = *image_partition.ElementsInPart(part_index).begin();
    return;
  }

  // Variant 2).
  *base_node = *base_partition.ElementsInPart(part_index).begin();
  if (image_partition.PartOf(*base_node) == part_index) {
    *image_node = *base_node;
  } else {
    *image_node = *image_partition.ElementsInPart(part_index).begin();
  }
}
}  // namespace

// TODO(user): refactor this method and its submethods into a dedicated class
// whose members will be ominously accessed by all the class methods; most
// notably the search state stack. This may improve readability.
std::unique_ptr<SparsePermutation>
GraphSymmetryFinder::FindOneSuitablePermutation(
    int root_node, int root_image_node, DynamicPartition* base_partition,
    DynamicPartition* image_partition,
    const std::vector<std::unique_ptr<SparsePermutation>>&
        generators_found_so_far,
    const std::vector<std::vector<int>>& permutations_displacing_node) {
  // DCHECKs() and statistics.
  ScopedTimeDistributionUpdater search_time_updater(&stats_.search_time);
  DCHECK_EQ("", tmp_dynamic_permutation_.DebugString());
  DCHECK_EQ(base_partition->DebugString(DynamicPartition::SORT_BY_PART),
            image_partition->DebugString(DynamicPartition::SORT_BY_PART));
  DCHECK(search_states_.empty());

  // These will be used during the search. See their usage.
  std::vector<int> base_singletons;
  std::vector<int> image_singletons;
  int next_base_node;
  int next_image_node;
  int min_potential_mismatching_part_index;
  std::vector<int> next_potential_image_nodes;

  // Initialize the search: we can already distinguish "root_node" in the base
  // partition. See the comment below.
  search_states_.emplace_back(
      /*base_node=*/root_node, /*first_image_node=*/-1,
      /*num_parts_before_trying_to_map_base_node=*/base_partition->NumParts(),
      /*min_potential_mismatching_part_index=*/base_partition->NumParts());
  // We inject the image node directly as the "remaining_pruned_image_nodes".
  search_states_.back().remaining_pruned_image_nodes.assign(1, root_image_node);
  {
    ScopedTimeDistributionUpdater u(&stats_.initial_search_refine_time);
    DistinguishNodeInPartition(root_node, base_partition, &base_singletons);
  }
  while (!search_states_.empty()) {
    if (time_limit_->LimitReached()) return nullptr;
    // When exploring a SearchState "ss", we're supposed to have:
    // - A base_partition that has already been refined on ss->base_node.
    //   (base_singleton is the list of singletons created on the base
    //    partition during that refinement).
    // - A non-empty list of potential image nodes (we'll try them in reverse
    //   order).
    // - An image partition that hasn't been refined yet.
    //
    // Also, one should note that the base partition (before its refinement on
    // base_node) was deemed compatible with the image partition as it is now.
    const SearchState& ss = search_states_.back();
    const int image_node = ss.first_image_node >= 0
                               ? ss.first_image_node
                               : ss.remaining_pruned_image_nodes.back();

    // Statistics, DCHECKs.
    IF_STATS_ENABLED(stats_.search_depth.Add(search_states_.size()));
    DCHECK_EQ(ss.num_parts_before_trying_to_map_base_node,
              image_partition->NumParts());

    // Apply the decision: map base_node to image_node. Since base_partition
    // was already refined on base_node, we just need to refine image_partition.
    {
      ScopedTimeDistributionUpdater u(&stats_.search_refine_time);
      DistinguishNodeInPartition(image_node, image_partition,
                                 &image_singletons);
    }
    VLOG(4) << ss.DebugString();
    VLOG(4) << base_partition->DebugString(DynamicPartition::SORT_BY_PART);
    VLOG(4) << image_partition->DebugString(DynamicPartition::SORT_BY_PART);

    // Run some diagnoses on the two partitions. There are many outcomes, so
    // it's a bit complicated:
    // 1) The partitions are incompatible
    //   - Because of a straightfoward criterion (size mismatch).
    //   - Because they are both fully refined (i.e. singletons only), yet the
    //     permutation induced by them is not a graph automorpshim.
    // 2) The partitions induce a permutation (all their non-singleton parts are
    //    identical), and this permutation is a graph automorphism.
    // 3) The partitions need further refinement:
    //    - Because some non-singleton parts aren't equal in the base and image
    //      partition
    //    - Or because they are a full match (i.e. may induce a permutation,
    //      like in 2)), but the induced permutation isn't a graph automorphism.
    bool compatible = true;
    {
      ScopedTimeDistributionUpdater u(&stats_.quick_compatibility_time);
      compatible = PartitionsAreCompatibleAfterPartIndex(
          *base_partition, *image_partition,
          ss.num_parts_before_trying_to_map_base_node);
      u.AlsoUpdate(compatible ? &stats_.quick_compatibility_success_time
                              : &stats_.quick_compatibility_fail_time);
    }
    bool partitions_are_full_match = false;
    if (compatible) {
      {
        ScopedTimeDistributionUpdater u(
            &stats_.dynamic_permutation_refinement_time);
        tmp_dynamic_permutation_.AddMappings(base_singletons, image_singletons);
      }
      ScopedTimeDistributionUpdater u(&stats_.map_election_std_time);
      min_potential_mismatching_part_index =
          ss.min_potential_mismatching_part_index;
      partitions_are_full_match = ConfirmFullMatchOrFindNextMappingDecision(
          *base_partition, *image_partition, tmp_dynamic_permutation_,
          &min_potential_mismatching_part_index, &next_base_node,
          &next_image_node);
      u.AlsoUpdate(partitions_are_full_match
                       ? &stats_.map_election_std_full_match_time
                       : &stats_.map_election_std_mapping_time);
    }
    if (compatible && partitions_are_full_match) {
      DCHECK_EQ(min_potential_mismatching_part_index,
                base_partition->NumParts());
      // We have a permutation candidate!
      // Note(user): we also deal with (extremely rare) false positives for
      // "partitions_are_full_match" here: in case they aren't a full match,
      // IsGraphAutomorphism() will catch that; and we'll simply deepen the
      // search.
      bool is_automorphism = true;
      {
        ScopedTimeDistributionUpdater u(&stats_.automorphism_test_time);
        is_automorphism = IsGraphAutomorphism(tmp_dynamic_permutation_);
        u.AlsoUpdate(is_automorphism ? &stats_.automorphism_test_success_time
                                     : &stats_.automorphism_test_fail_time);
      }
      if (is_automorphism) {
        ScopedTimeDistributionUpdater u(&stats_.search_finalize_time);
        // We found a valid permutation. We can return it, but first we
        // must restore the partitions to their original state.
        std::unique_ptr<SparsePermutation> sparse_permutation(
            tmp_dynamic_permutation_.CreateSparsePermutation());
        VLOG(4) << "Automorphism found: " << sparse_permutation->DebugString();
        const int base_num_parts =
            search_states_[0].num_parts_before_trying_to_map_base_node;
        base_partition->UndoRefineUntilNumPartsEqual(base_num_parts);
        image_partition->UndoRefineUntilNumPartsEqual(base_num_parts);
        tmp_dynamic_permutation_.Reset();
        search_states_.clear();

        search_time_updater.AlsoUpdate(&stats_.search_time_success);
        return sparse_permutation;
      }

      // The permutation isn't a valid automorphism. Either the partitions were
      // fully refined, and we deem them incompatible, or they weren't, and we
      // consider them as 'not a full match'.
      VLOG(4) << "Permutation candidate isn't a valid automorphism.";
      if (base_partition->NumParts() == NumNodes()) {
        // Fully refined: the partitions are incompatible.
        compatible = false;
        ScopedTimeDistributionUpdater u(&stats_.dynamic_permutation_undo_time);
        tmp_dynamic_permutation_.UndoLastMappings(&base_singletons);
      } else {
        ScopedTimeDistributionUpdater u(&stats_.map_reelection_time);
        // TODO(user): try to get the non-singleton part from
        // DynamicPermutation in O(1), if it matters to the overall speed.
        int non_singleton_part = 0;
        {
          ScopedTimeDistributionUpdater u(&stats_.non_singleton_search_time);
          while (base_partition->SizeOfPart(non_singleton_part) == 1) {
            ++non_singleton_part;
            DCHECK_LT(non_singleton_part, base_partition->NumParts());
          }
        }
        // The partitions are compatible, but we'll deepen the search on some
        // non-singleton part. We can pick any base and image node in this case.
        GetBestMapping(*base_partition, *image_partition, non_singleton_part,
                       &next_base_node, &next_image_node);
      }
    }

    // Now we've fully diagnosed our partitions, and have already dealt with
    // case 2). We're left to deal with 1) and 3).
    //
    // Case 1): partitions are incompatible.
    if (!compatible) {
      ScopedTimeDistributionUpdater u(&stats_.backtracking_time);
      // We invalidate the current image node, and prune the remaining image
      // nodes. We might be left with no other image nodes, which means that
      // we'll backtrack, i.e. pop our current SearchState and invalidate the
      // 'current' image node of the upper SearchState (which might lead to us
      // backtracking it, and so on).
      while (!search_states_.empty()) {
        SearchState* const last_ss = &search_states_.back();
        image_partition->UndoRefineUntilNumPartsEqual(
            last_ss->num_parts_before_trying_to_map_base_node);
        if (last_ss->first_image_node >= 0) {
          // Find out and prune the remaining potential image nodes: there is
          // no permutation that maps base_node -> image_node that is
          // compatible with the current partition, so there can't be a
          // permutation that maps base_node -> X either, for all X in the orbit
          // of 'image_node' under valid permutations compatible with the
          // current partition. Ditto for other potential image nodes.
          //
          // TODO(user): fix this: we should really be collecting all
          // permutations displacing any node in "image_part", for the pruning
          // to be really exhaustive. We could also consider alternative ways,
          // like incrementally maintaining the list of permutations compatible
          // with the partition so far.
          const int part = image_partition->PartOf(last_ss->first_image_node);
          last_ss->remaining_pruned_image_nodes.reserve(
              image_partition->SizeOfPart(part));
          last_ss->remaining_pruned_image_nodes.push_back(
              last_ss->first_image_node);
          for (const int e : image_partition->ElementsInPart(part)) {
            if (e != last_ss->first_image_node) {
              last_ss->remaining_pruned_image_nodes.push_back(e);
            }
          }
          {
            ScopedTimeDistributionUpdater u(&stats_.pruning_time);
            PruneOrbitsUnderPermutationsCompatibleWithPartition(
                *image_partition, generators_found_so_far,
                permutations_displacing_node[last_ss->first_image_node],
                &last_ss->remaining_pruned_image_nodes);
          }
          SwapFrontAndBack(&last_ss->remaining_pruned_image_nodes);
          DCHECK_EQ(last_ss->remaining_pruned_image_nodes.back(),
                    last_ss->first_image_node);
          last_ss->first_image_node = -1;
        }
        last_ss->remaining_pruned_image_nodes.pop_back();
        if (!last_ss->remaining_pruned_image_nodes.empty()) break;

        VLOG(4) << "Backtracking one level up.";
        base_partition->UndoRefineUntilNumPartsEqual(
            last_ss->num_parts_before_trying_to_map_base_node);
        // If this was the root search state (i.e. we fully backtracked and
        // will exit the search after that), we don't have mappings to undo.
        // We run UndoLastMappings() anyway, because it's a no-op in that case.
        tmp_dynamic_permutation_.UndoLastMappings(&base_singletons);
        search_states_.pop_back();
      }
      // Continue the search.
      continue;
    }

    // Case 3): we deepen the search.
    // Since the search loop starts from an already-refined base_partition,
    // we must do it here.
    VLOG(4) << "    Deepening the search.";
    search_states_.emplace_back(
        next_base_node, next_image_node,
        /*num_parts_before_trying_to_map_base_node*/ base_partition->NumParts(),
        min_potential_mismatching_part_index);
    {
      ScopedTimeDistributionUpdater u(&stats_.search_refine_time);
      DistinguishNodeInPartition(next_base_node, base_partition,
                                 &base_singletons);
    }
  }
  // We exhausted the search; we didn't find any permutation.
  search_time_updater.AlsoUpdate(&stats_.search_time_fail);
  return nullptr;
}

BeginEndWrapper<std::vector<int>::const_iterator>
GraphSymmetryFinder::TailsOfIncomingArcsTo(int node) const {
  return BeginEndWrapper<std::vector<int>::const_iterator>(
      flattened_reverse_adj_lists_.begin() + reverse_adj_list_index_[node],
      flattened_reverse_adj_lists_.begin() + reverse_adj_list_index_[node + 1]);
}

void GraphSymmetryFinder::PruneOrbitsUnderPermutationsCompatibleWithPartition(
    const DynamicPartition& partition,
    const std::vector<std::unique_ptr<SparsePermutation>>& permutations,
    const std::vector<int>& permutation_indices, std::vector<int>* nodes) {
  VLOG(4) << "    Pruning [" << strings::Join(*nodes, ", ") << "]";
  // TODO(user): apply a smarter test to decide whether to do the pruning
  // or not: we can accurately estimate the cost of pruning (iterate through
  // all generators found so far) and its estimated benefit (the cost of
  // the search below the state that we're currently in, times the expected
  // number of pruned nodes). Sometimes it may be better to skip the
  // pruning.
  if (nodes->size() <= 1) return;

  // Iterate on all targeted permutations. If they are compatible, apply
  // them to tmp_partition_ which will contain the incrementally merged
  // equivalence classes.
  std::vector<int>& tmp_nodes_on_support =
      tmp_stack_;  // Rename, for readability.
  DCHECK(tmp_nodes_on_support.empty());
  // TODO(user): investigate further optimizations: maybe it's possible
  // to incrementally maintain the set of permutations that is compatible
  // with the current partition, instead of recomputing it here?
  for (const int p : permutation_indices) {
    const SparsePermutation& permutation = *permutations[p];
    // First, a quick compatibility check: the permutation's cycles must be
    // smaller or equal to the size of the part that they are included in.
    bool compatible = true;
    for (int c = 0; c < permutation.NumCycles(); ++c) {
      const SparsePermutation::Iterator cycle = permutation.Cycle(c);
      if (cycle.size() >
          partition.SizeOfPart(partition.PartOf(*cycle.begin()))) {
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
        if (partition.PartOf(node) != part) {
          if (part >= 0) {
            compatible = false;
            break;
          }
          part = partition.PartOf(node);  // Initilization of 'part'.
        }
      }
    }
    if (!compatible) continue;
    // The permutation is fully compatible!
    // TODO(user): ignore cycles that are outside of image_part.
    MergeNodeEquivalenceClassesAccordingToPermutation(permutation,
                                                      &tmp_partition_, nullptr);
    for (const int node : permutation.Support()) {
      if (!tmp_node_mask_[node]) {
        tmp_node_mask_[node] = true;
        tmp_nodes_on_support.push_back(node);
      }
    }
  }

  // Apply the pruning.
  tmp_partition_.KeepOnlyOneNodePerPart(nodes);

  // Reset the "tmp_" structures sparsely.
  for (const int node : tmp_nodes_on_support) {
    tmp_node_mask_[node] = false;
    tmp_partition_.ResetNode(node);
  }
  tmp_nodes_on_support.clear();
  VLOG(4) << "    Pruned: [" << strings::Join(*nodes, ", ") << "]";
}

bool GraphSymmetryFinder::ConfirmFullMatchOrFindNextMappingDecision(
    const DynamicPartition& base_partition,
    const DynamicPartition& image_partition,
    const DynamicPermutation& current_permutation_candidate,
    int* min_potential_mismatching_part_index_io, int* next_base_node,
    int* next_image_node) const {
  *next_base_node = -1;
  *next_image_node = -1;

  // The following clause should be true most of the times, except in some
  // specific use cases.
  if (!FLAGS_minimize_permutation_support_size) {
    // First, we try to map the loose ends of the current permutations: these
    // loose ends can't be mapped to themselves, so we'll have to map them to
    // something anyway.
    for (const int loose_node : current_permutation_candidate.LooseEnds()) {
      DCHECK_GT(base_partition.ElementsInSamePartAs(loose_node).size(), 1);
      *next_base_node = loose_node;
      const int root = current_permutation_candidate.RootOf(loose_node);
      DCHECK_NE(root, loose_node);
      if (image_partition.PartOf(root) == base_partition.PartOf(loose_node)) {
        // We prioritize mapping a loose end to its own root (i.e. close a
        // cycle), if possible, like here: we exit immediately.
        *next_image_node = root;
        return false;
      }
    }
    if (*next_base_node != -1) {
      // We found loose ends, but none that mapped to its own root. Just pick
      // any valid image.
      *next_image_node =
          *image_partition.ElementsInPart(
                               base_partition.PartOf(*next_base_node)).begin();
      return false;
    }
  }

  // If there is no loose node (i.e. the current permutation only has closed
  // cycles), we fall back to picking any part that is different in the base and
  // image partitions; because we know that some mapping decision will have to
  // be made there.
  // SUBTLE: we use "min_potential_mismatching_part_index_io" to incrementally
  // keep running this search (for a mismatching part) from where we left off.
  // TODO(user): implement a simpler search for a mismatching part: it's
  // trivially possible if the base partition maintains a hash set of all
  // Fprints of its parts, and if the image partition uses that to maintain the
  // list of 'different' non-singleton parts.
  const int initial_min_potential_mismatching_part_index =
      *min_potential_mismatching_part_index_io;
  for (; *min_potential_mismatching_part_index_io < base_partition.NumParts();
       ++*min_potential_mismatching_part_index_io) {
    const int p = *min_potential_mismatching_part_index_io;
    if (base_partition.SizeOfPart(p) != 1 &&
        base_partition.FprintOfPart(p) != image_partition.FprintOfPart(p)) {
      GetBestMapping(base_partition, image_partition, p, next_base_node,
                     next_image_node);
      return false;
    }

    const int parent = base_partition.ParentOfPart(p);
    if (parent < initial_min_potential_mismatching_part_index &&
        base_partition.SizeOfPart(parent) != 1 &&
        base_partition.FprintOfPart(parent) !=
            image_partition.FprintOfPart(parent)) {
      GetBestMapping(base_partition, image_partition, parent, next_base_node,
                     next_image_node);
      return false;
    }
  }

  // We didn't find an unequal part. DCHECK that our "incremental" check was
  // actually correct and that all non-singleton parts are indeed equal.
  if (DEBUG_MODE) {
    for (int p = 0; p < base_partition.NumParts(); ++p) {
      if (base_partition.SizeOfPart(p) != 1) {
        CHECK_EQ(base_partition.FprintOfPart(p),
                 image_partition.FprintOfPart(p));
      }
    }
  }
  return true;
}

std::string GraphSymmetryFinder::SearchState::DebugString() const {
  return StringPrintf(
      "SearchState{ base_node=%d, first_image_node=%d,"
      " remaining_pruned_image_nodes=[%s],"
      " num_parts_before_trying_to_map_base_node=%d }",
      base_node, first_image_node,
      strings::Join(remaining_pruned_image_nodes, " ").c_str(),
      num_parts_before_trying_to_map_base_node);
}

}  // namespace operations_research
