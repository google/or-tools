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

// TODO(user,user): refine this toplevel comment when this file settles.
//
// Two dynamic partition classes: one that incrementally splits a partition
// into more and more parts; one that incrementally merges a partition into less
// and less parts.
//
// GLOSSARY:
// The partition classes maintain a partition of N integers 0..N-1
// (aka "elements") into disjoint equivalence classes (aka "parts").
//
// SAFETY:
// Like vector<int> crashes when used improperly, these classes are not "safe":
// most of their methods may crash if called with invalid arguments. The client
// code is responsible for using this class properly. A few DCHECKs() will help
// catch bugs, though.

#ifndef OR_TOOLS_ALGORITHMS_DYNAMIC_PARTITION_H_
#define OR_TOOLS_ALGORITHMS_DYNAMIC_PARTITION_H_

#include <string>
#include <vector>

#include "ortools/base/logging.h"

namespace operations_research {

// Partition class that supports incremental splitting, with backtracking.
// See http://en.wikipedia.org/wiki/Partition_refinement .
// More precisely, the supported edit operations are:
// - Refine the partition so that a subset S (typically, |S| <<< N)
//   of elements are all considered non-equivalent to any element in ¬S.
//   Typically, this should be done in O(|S|).
// - Undo the above operations (backtracking).
//
// TODO(user): rename this to BacktrackableSplittingPartition.
class DynamicPartition {
 public:
  // Creates a DynamicPartition on n elements, numbered 0..n-1. Start with
  // the trivial partition (only one subset containing all elements).
  explicit DynamicPartition(int num_elements);

  // Ditto, but specify the initial part of each elements. Part indices must
  // form a dense integer set starting at 0; eg. [2, 1, 0, 1, 1, 3, 0] is valid.
  explicit DynamicPartition(const std::vector<int>& initial_part_of_element);

  // Accessors.
  int NumElements() const { return element_.size(); }
  const int NumParts() const { return part_.size(); }

  // To iterate over the elements in part #i:
  // for (int element : partition.ElementsInPart(i)) { ... }
  //
  // ORDERING OF ELEMENTS INSIDE PARTS: the order of elements within a given
  // part is volatile, and may change with Refine() or UndoRefine*() operations,
  // even if the part itself doesn't change.
  struct IterablePart;
  IterablePart ElementsInPart(int i) const;

  int PartOf(int element) const;
  int SizeOfPart(int part) const;
  int ParentOfPart(int part) const;

  // A handy shortcut to ElementsInPart(PartOf(e)). The returned IterablePart
  // will never be empty, since it contains at least i.
  IterablePart ElementsInSamePartAs(int i) const;

  // Returns a fingerprint of the given part. While collisions are possible,
  // their probability is quite low. Two parts that have the same size and the
  // same fingerprint are most likely identical.
  // Also, two parts that have the exact same set of elements will *always*
  // have the same fingerprint.
  uint64 FprintOfPart(int part) const;

  // Refines the partition such that elements that are in distinguished_subset
  // never share the same part as elements that aren't in that subset.
  // This might be a no-op: in that case, NumParts() won't change, but the
  // order of elements inside each part may change.
  //
  // ORDERING OF PARTS:
  // For each i such that Part #i has a non-trivial intersection with
  // "distinguished_subset" (neither empty, nor the full Part); Part #i is
  // stripped out of all elements that are in "distinguished_subset", and
  // those elements are sent to a newly created part, whose parent_part = i.
  // The parts newly created by a single Refine() operations are sorted
  // by parent_part.
  // Example: a Refine() on a partition with 6 parts causes parts #1, #3 and
  // #4 to be split: the partition will now contain 3 new parts: part #6 (with
  // parent_part = 1), part #7 (with parent_part = 3) and part #8 (with
  // parent_part = 4).
  //
  // TODO(user): the graph symmetry finder could probably benefit a lot from
  // keeping track of one additional bit of information for each part that
  // remains unchanged by a Refine() operation: was that part entirely *in*
  // the distinguished subset or entirely *out*?
  void Refine(const std::vector<int>& distinguished_subset);

  // Undo one or several Refine() operations, until the number of parts
  // becomes equal to "original_num_parts".
  // Prerequisite: NumParts() >= original_num_parts.
  void UndoRefineUntilNumPartsEqual(int original_num_parts);

  // Dump the partition to a string. There might be different conventions for
  // sorting the parts and the elements inside them.
  enum DebugStringSorting {
    // Elements are sorted within parts, and parts are then sorted
    // lexicographically.
    SORT_LEXICOGRAPHICALLY,
    // Elements are sorted within parts, and parts are kept in order.
    SORT_BY_PART,
  };
  std::string DebugString(DebugStringSorting sorting) const;

  // ADVANCED USAGE:
  // All elements (0..n-1) of the partition, sorted in a way that's compatible
  // with the hierarchical partitioning:
  // - All the elements of any given part are contiguous.
  // - Elements of a part P are always after elements of part Parent(P).
  // - The order remains identical (and the above property holds) after any
  //   UndoRefine*() operation.
  // Note that the order does get changed by Refine() operations.
  // This is a reference, so it'll only remain valid and constant until the
  // class is destroyed or until Refine() get called.
  const std::vector<int>& ElementsInHierarchicalOrder() const {
    return element_;
  }

 private:
  // A DynamicPartition instance maintains a list of all of its elements,
  // 'sorted' by partitions: elements of the same subset are contiguous
  // in that list.
  std::vector<int> element_;

  // The reverse of elements_[]: element_[index_of_[i]] = i.
  std::vector<int> index_of_;

  // part_of_[i] is the index of the part that contains element i.
  std::vector<int> part_of_;

  struct Part {
    // This part holds elements[start_index .. end_index-1].
    // INVARIANT: end_index > start_index.
    int start_index;  // Inclusive
    int end_index;    // Exclusive

    // The Part that this part was split out of. See the comment at Refine().
    // INVARIANT: part[i].parent_part <= i, and the equality holds iff part[i]
    // has no parent.
    int parent_part;  // Index into the part[] array.

    // The part's fingerprint is the XOR of all fingerprints of its elements.
    // See FprintOfInt32() in the .cc.
    uint64 fprint;

    Part() : start_index(0), end_index(0), parent_part(0), fprint(0) {}
    Part(int start_index, int end_index, int parent_part, uint64 fprint)
        : start_index(start_index),
          end_index(end_index),
          parent_part(parent_part),
          fprint(fprint) {}
  };
  std::vector<Part> part_;  // The disjoint parts.

  // Used temporarily and exclusively by Refine(). This prevents Refine()
  // from being thread-safe.
  // INVARIANT: tmp_counter_of_part_ contains only 0s before and after Refine().
  std::vector<int> tmp_counter_of_part_;
  std::vector<int> tmp_affected_parts_;
};

struct DynamicPartition::IterablePart {
  std::vector<int>::const_iterator begin() const { return begin_; }
  std::vector<int>::const_iterator end() const { return end_; }
  std::vector<int>::const_iterator begin_;
  std::vector<int>::const_iterator end_;

  int size() const { return end_ - begin_; }

  IterablePart() {}
  IterablePart(const std::vector<int>::const_iterator& b,
               const std::vector<int>::const_iterator& e)
      : begin_(b), end_(e) {}

  // These typedefs allow this iterator to be used within testing::ElementsAre.
  typedef int value_type;
  typedef std::vector<int>::const_iterator const_iterator;
};

// Partition class that supports incremental merging, using the union-find
// algorithm (see http://en.wikipedia.org/wiki/Disjoint-set_data_structure).
class MergingPartition {
 public:
  // At first, all nodes are in their own singleton part.
  MergingPartition() { Reset(0); }
  explicit MergingPartition(int num_nodes) { Reset(num_nodes); }
  void Reset(int num_nodes);

  int NumNodes() const { return parent_.size(); }

  // Complexity: amortized O(Ackermann⁻¹(N)) -- which is essentially O(1) --
  // where N is the number of nodes.
  //
  // Return value: If this merge caused a representative node (of either node1
  // or node2) to stop being a representative (because only one can remain);
  // this method returns that removed representative. Otherwise it returns -1.
  //
  // Details: a smaller part will always be merged onto a larger one.
  // Upons ties, the smaller representative becomes the overall representative.
  int MergePartsOf(int node1, int node2);  // The 'union' of the union-find.

  // Get the representative of "node" (a node in the same equivalence class,
  // which will also be returned for any other "node" in the same class).
  // The complexity if the same as MergePartsOf().
  int GetRootAndCompressPath(int node);

  // Specialized reader API: prunes "nodes" to only keep at most one node per
  // part: any node which is in the same part as an earlier node will be pruned.
  void KeepOnlyOneNodePerPart(std::vector<int>* nodes);

  // Output the whole partition as node equivalence classes: if there are K
  // parts and N nodes, node_equivalence_classes[i] will contain the part index
  // (a number in 0..K-1) of node #i. Parts will be sorted by their first node
  // (i.e. node 0 will always be in part 0; then the next node that isn't in
  // part 0 will be in part 1, and so on).
  // Returns the number K of classes.
  int FillEquivalenceClasses(std::vector<int>* node_equivalence_classes);

  // Dump all components, with nodes sorted within each part and parts
  // sorted lexicographically. Eg. "0 1 3 4 | 2 5 | 6 7 8".
  std::string DebugString();

  // Advanced usage: sets 'node' to be in its original singleton. All nodes
  // who may point to 'node' as a parent will remain in an inconsistent state.
  // This can be used to reinitialize a MergingPartition that has been sparsely
  // modified in O(|modifications|).
  // CRASHES IF USED INCORRECTLY.
  void ResetNode(int node);

  int NumNodesInSamePartAs(int node) {
    return part_size_[GetRootAndCompressPath(node)];
  }

  // FOR DEBUGGING OR SPECIAL "CONST" ACCESS ONLY:
  // Find the root of the union-find tree with leaf 'node', i.e. its
  // representative node, but don't use path compression.
  // The amortized complexity can be as bad as log(N), as opposed to the
  // version using path compression.
  int GetRoot(int node) const;

 private:
  // Along the upwards path from 'node' to its root, set the parent of all
  // nodes (including the root) to 'parent'.
  void SetParentAlongPathToRoot(int node, int parent);

  std::vector<int> parent_;
  std::vector<int> part_size_;

  // Used transiently by KeepOnlyOneNodePerPart().
  std::vector<bool> tmp_part_bit_;
};

// *** Implementation of inline methods of the above classes. ***

inline DynamicPartition::IterablePart DynamicPartition::ElementsInPart(
    int i) const {
  DCHECK_GE(i, 0);
  DCHECK_LT(i, NumParts());
  return IterablePart(element_.begin() + part_[i].start_index,
                      element_.begin() + part_[i].end_index);
}

inline int DynamicPartition::PartOf(int element) const {
  DCHECK_GE(element, 0);
  DCHECK_LT(element, part_of_.size());
  return part_of_[element];
}

inline int DynamicPartition::SizeOfPart(int part) const {
  DCHECK_GE(part, 0);
  DCHECK_LT(part, part_.size());
  const Part& p = part_[part];
  return p.end_index - p.start_index;
}

inline int DynamicPartition::ParentOfPart(int part) const {
  DCHECK_GE(part, 0);
  DCHECK_LT(part, part_.size());
  return part_[part].parent_part;
}

inline DynamicPartition::IterablePart DynamicPartition::ElementsInSamePartAs(
    int i) const {
  return ElementsInPart(PartOf(i));
}

inline uint64 DynamicPartition::FprintOfPart(int part) const {
  DCHECK_GE(part, 0);
  DCHECK_LT(part, part_.size());
  return part_[part].fprint;
}

inline int MergingPartition::GetRoot(int node) const {
  DCHECK_GE(node, 0);
  DCHECK_LT(node, NumNodes());
  int child = node;
  while (true) {
    const int parent = parent_[child];
    if (parent == child) return child;
    child = parent;
  }
}

inline void MergingPartition::SetParentAlongPathToRoot(int node, int parent) {
  DCHECK_GE(node, 0);
  DCHECK_LT(node, NumNodes());
  DCHECK_GE(parent, 0);
  DCHECK_LT(parent, NumNodes());
  int child = node;
  while (true) {
    const int old_parent = parent_[child];
    parent_[child] = parent;
    if (old_parent == child) return;
    child = old_parent;
  }
}

inline void MergingPartition::ResetNode(int node) {
  DCHECK_GE(node, 0);
  DCHECK_LT(node, NumNodes());
  parent_[node] = node;
  part_size_[node] = 1;
}

}  // namespace operations_research

#endif  // OR_TOOLS_ALGORITHMS_DYNAMIC_PARTITION_H_
