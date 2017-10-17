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

#include "ortools/algorithms/dynamic_partition.h"

#include <algorithm>

#include "ortools/base/stringprintf.h"
#include "ortools/base/join.h"
#include "ortools/base/murmur.h"

namespace operations_research {

namespace {
uint64 FprintOfInt32(int i) {
  return util_hash::MurmurHash64(reinterpret_cast<const char*>(&i),
                                 sizeof(int));
}
}  // namespace

DynamicPartition::DynamicPartition(int num_elements) {
  DCHECK_GE(num_elements, 0);
  element_.assign(num_elements, -1);
  index_of_.assign(num_elements, -1);
  for (int i = 0; i < num_elements; ++i) {
    element_[i] = i;
    index_of_[i] = i;
  }
  part_of_.assign(num_elements, 0);
  uint64 fprint = 0;
  for (int i = 0; i < num_elements; ++i) fprint ^= FprintOfInt32(i);
  part_.push_back(Part(/*start_index=*/0, /*end_index=*/num_elements,
                       /*parent_part=*/0,
                       /*fprint=*/fprint));
}

DynamicPartition::DynamicPartition(
    const std::vector<int>& initial_part_of_element) {
  if (initial_part_of_element.empty()) return;
  part_of_ = initial_part_of_element;
  const int n = part_of_.size();
  const int num_parts = 1 + *std::max_element(part_of_.begin(), part_of_.end());
  DCHECK_EQ(0, *std::min_element(part_of_.begin(), part_of_.end()));
  part_.resize(num_parts);

  // Compute the part fingerprints.
  for (int i = 0; i < n; ++i) part_[part_of_[i]].fprint ^= FprintOfInt32(i);

  // Compute the actual start indices of each part, knowing that we'll sort
  // them as they were given implicitly in "initial_part_of_element".
  // The code looks a bit weird to do it in-place, with no additional memory.
  for (int p = 0; p < num_parts; ++p) {
    part_[p].end_index = 0;  // Temporarily utilized as size_of_part.
    part_[p].parent_part = p;
  }
  for (const int p : part_of_) ++part_[p].end_index;  // size_of_part
  int sum_part_sizes = 0;
  for (int p = 0; p < num_parts; ++p) {
    part_[p].start_index = sum_part_sizes;
    sum_part_sizes += part_[p].end_index;  // size of part.
  }

  // Now that we have the correct start indices, we set the end indices to the
  // start indices, and incrementally add all elements to their part, adjusting
  // the end indices as we go.
  for (Part& part : part_) part.end_index = part.start_index;
  element_.assign(n, -1);
  index_of_.assign(n, -1);
  for (int element = 0; element < n; ++element) {
    Part* const part = &part_[part_of_[element]];
    element_[part->end_index] = element;
    index_of_[element] = part->end_index;
    ++part->end_index;
  }

  // Verify that we did it right.
  // TODO(user): either remove this or factor it out if it can be used
  // elsewhere.
  DCHECK_EQ(0, part_[0].start_index);
  DCHECK_EQ(NumElements(), part_[NumParts() - 1].end_index);
  for (int p = 1; p < NumParts(); ++p) {
    DCHECK_EQ(part_[p - 1].end_index, part_[p].start_index);
  }
}

void DynamicPartition::Refine(const std::vector<int>& distinguished_subset) {
  // tmp_counter_of_part_[i] will contain the number of
  // elements in distinguished_subset that were part of part #i.
  tmp_counter_of_part_.resize(NumParts(), 0);
  // We remember the Parts that were actually affected.
  tmp_affected_parts_.clear();
  for (const int element : distinguished_subset) {
    DCHECK_GE(element, 0);
    DCHECK_LT(element, NumElements());
    const int part = part_of_[element];
    const int num_distinguished_elements_in_part = ++tmp_counter_of_part_[part];
    // Is this the first time that we touch this element's part?
    if (num_distinguished_elements_in_part == 1) {
      // TODO(user): optimize the common singleton case.
      tmp_affected_parts_.push_back(part);
    }
    // Move the element to the end of its current Part.
    const int old_index = index_of_[element];
    const int new_index =
        part_[part].end_index - num_distinguished_elements_in_part;
    DCHECK_GE(new_index, old_index) << "Duplicate element given to Refine(): "
                                    << element;
    // Perform the swap, keeping index_of_ up to date.
    index_of_[element] = new_index;
    index_of_[element_[new_index]] = old_index;
    std::swap(element_[old_index], element_[new_index]);
  }

  // Sort affected parts. This is important to behave as advertised in the .h.
  // TODO(user, fdid): automatically switch to an O(N) sort when it's faster
  // than this one, which is O(K log K) with K = tmp_affected_parts_.size().
  std::sort(tmp_affected_parts_.begin(), tmp_affected_parts_.end());

  // Iterate on each affected part and split it, or keep it intact if all
  // of its elements were distinguished.
  for (const int part : tmp_affected_parts_) {
    const int start_index = part_[part].start_index;
    const int end_index = part_[part].end_index;
    const int split_index = end_index - tmp_counter_of_part_[part];
    tmp_counter_of_part_[part] = 0;  // Clean up after us.
    DCHECK_GE(split_index, start_index);
    DCHECK_LT(split_index, end_index);

    // Do nothing if all elements were distinguished.
    if (split_index == start_index) continue;

    // Compute the fingerprint of the new part.
    uint64 new_fprint = 0;
    for (int i = split_index; i < end_index; ++i) {
      new_fprint ^= FprintOfInt32(element_[i]);
    }

    const int new_part = NumParts();

    // Perform the split.
    part_[part].end_index = split_index;
    part_[part].fprint ^= new_fprint;
    part_.push_back(Part(/*start_index*/ split_index, /*end_index*/ end_index,
                         /*parent_part*/ part, new_fprint));
    for (const int element : ElementsInPart(new_part)) {
      part_of_[element] = new_part;
    }
  }
}

void DynamicPartition::UndoRefineUntilNumPartsEqual(int original_num_parts) {
  DCHECK_GE(NumParts(), original_num_parts);
  DCHECK_GE(original_num_parts, 1);
  while (NumParts() > original_num_parts) {
    const int part_index = NumParts() - 1;
    const Part& part = part_[part_index];
    const int parent_part_index = part.parent_part;
    DCHECK_LT(parent_part_index, part_index) << "UndoRefineUntilNumPartsEqual()"
                                                " called with "
                                                "'original_num_parts' too low";

    // Update the part contents: actually merge "part" onto its parent.
    for (const int element : ElementsInPart(part_index)) {
      part_of_[element] = parent_part_index;
    }
    Part* const parent_part = &part_[parent_part_index];
    DCHECK_EQ(part.start_index, parent_part->end_index);
    parent_part->end_index = part.end_index;
    parent_part->fprint ^= part.fprint;
    part_.pop_back();
  }
}

std::string DynamicPartition::DebugString(DebugStringSorting sorting) const {
  if (sorting != SORT_LEXICOGRAPHICALLY && sorting != SORT_BY_PART) {
    return StringPrintf("Unsupported sorting: %d", sorting);
  }
  std::vector<std::vector<int>> parts;
  for (int i = 0; i < NumParts(); ++i) {
    IterablePart iterable_part = ElementsInPart(i);
    parts.emplace_back(iterable_part.begin(), iterable_part.end());
    std::sort(parts.back().begin(), parts.back().end());
  }
  if (sorting == SORT_LEXICOGRAPHICALLY) {
    std::sort(parts.begin(), parts.end());
  }
  std::string out;
  for (const std::vector<int>& part : parts) {
    if (!out.empty()) out += " | ";
    out += strings::Join(part, " ");
  }
  return out;
}

void MergingPartition::Reset(int num_nodes) {
  DCHECK_GE(num_nodes, 0);
  part_size_.assign(num_nodes, 1);
  parent_.assign(num_nodes, -1);
  for (int i = 0; i < num_nodes; ++i) parent_[i] = i;
  tmp_part_bit_.assign(num_nodes, false);
}

int MergingPartition::MergePartsOf(int node1, int node2) {
  DCHECK_GE(node1, 0);
  DCHECK_GE(node2, 0);
  DCHECK_LT(node1, NumNodes());
  DCHECK_LT(node2, NumNodes());
  int root1 = GetRoot(node1);
  int root2 = GetRoot(node2);
  if (root1 == root2) return -1;
  int s1 = part_size_[root1];
  int s2 = part_size_[root2];
  // Attach the smaller part to the larger one. Break ties by root index.
  if (s1 < s2 || (s1 == s2 && root1 > root2)) {
    std::swap(root1, root2);
    std::swap(s1, s2);
  }

  // Update the part size. Don't change part_size_[root2]: it won't be used
  // again by further merges.
  part_size_[root1] += part_size_[root2];
  SetParentAlongPathToRoot(node1, root1);
  SetParentAlongPathToRoot(node2, root1);
  return root2;
}

int MergingPartition::GetRootAndCompressPath(int node) {
  DCHECK_GE(node, 0);
  DCHECK_LT(node, NumNodes());
  const int root = GetRoot(node);
  SetParentAlongPathToRoot(node, root);
  return root;
}

void MergingPartition::KeepOnlyOneNodePerPart(std::vector<int>* nodes) {
  int num_nodes_kept = 0;
  for (const int node : *nodes) {
    const int representative = GetRootAndCompressPath(node);
    if (!tmp_part_bit_[representative]) {
      tmp_part_bit_[representative] = true;
      (*nodes)[num_nodes_kept++] = node;
    }
  }
  nodes->resize(num_nodes_kept);

  // Clean up the tmp_part_bit_ vector. Since we've already compressed the
  // paths (if backtracking was enabled), no need to do it again.
  for (const int node : *nodes) tmp_part_bit_[GetRoot(node)] = false;
}

int MergingPartition::FillEquivalenceClasses(
    std::vector<int>* node_equivalence_classes) {
  node_equivalence_classes->assign(NumNodes(), -1);
  int num_roots = 0;
  for (int node = 0; node < NumNodes(); ++node) {
    const int root = GetRootAndCompressPath(node);
    if ((*node_equivalence_classes)[root] < 0) {
      (*node_equivalence_classes)[root] = num_roots;
      ++num_roots;
    }
    (*node_equivalence_classes)[node] = (*node_equivalence_classes)[root];
  }
  return num_roots;
}

std::string MergingPartition::DebugString() {
  std::vector<std::vector<int>> sorted_parts(NumNodes());
  for (int i = 0; i < NumNodes(); ++i) {
    sorted_parts[GetRootAndCompressPath(i)].push_back(i);
  }
  for (std::vector<int>& part : sorted_parts)
    std::sort(part.begin(), part.end());
  std::sort(sorted_parts.begin(), sorted_parts.end());
  // Note: typically, a lot of elements of "sorted_parts" will be empty,
  // but these won't be visible in the std::string that we construct below.
  std::string out;
  for (const std::vector<int>& part : sorted_parts) {
    if (!out.empty()) out += " | ";
    out += strings::Join(part, " ");
  }
  return out;
}

}  // namespace operations_research
