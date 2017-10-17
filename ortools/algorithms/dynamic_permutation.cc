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

#include "ortools/algorithms/dynamic_permutation.h"

#include <algorithm>
#include "ortools/algorithms/sparse_permutation.h"

namespace operations_research {

DynamicPermutation::DynamicPermutation(int n)
    : image_(n, -1), ancestor_(n, -1), tmp_mask_(n, false) {
  for (int i = 0; i < Size(); ++i) image_[i] = ancestor_[i] = i;
}

void DynamicPermutation::AddMappings(const std::vector<int>& src,
                                     const std::vector<int>& dst) {
  DCHECK_EQ(src.size(), dst.size());
  mapping_src_size_stack_.push_back(mapping_src_stack_.size());
  mapping_src_stack_.reserve(mapping_src_stack_.size() + src.size());
  for (int i = 0; i < src.size(); ++i) {
    const int s = src[i];
    const int d = dst[i];
    DCHECK_EQ(s, ImageOf(s));    // No prior image of s.
    DCHECK_EQ(d, ancestor_[d]);  // No prior ancestor of d.

    ancestor_[d] = RootOf(s);
    image_[s] = d;

    if (image_[d] == d) loose_ends_.insert(d);
    loose_ends_.erase(s);  // Also takes care of the corner case s == d.

    // Remember the sources for the undo stack.
    mapping_src_stack_.push_back(s);
  }
}

void DynamicPermutation::UndoLastMappings(
    std::vector<int>* undone_mapping_src) {
  DCHECK(undone_mapping_src != nullptr);
  undone_mapping_src->clear();
  if (mapping_src_size_stack_.empty()) return;  // Nothing to undo.
  const int num_mappings_before = mapping_src_size_stack_.back();
  mapping_src_size_stack_.pop_back();
  const int num_mappings_now = mapping_src_stack_.size();
  DCHECK_GE(num_mappings_now, num_mappings_before);
  // Dump the undone mappings.
  undone_mapping_src->reserve(num_mappings_now - num_mappings_before);
  undone_mapping_src->insert(undone_mapping_src->begin(),
                             mapping_src_stack_.begin() + num_mappings_before,
                             mapping_src_stack_.end());
  // Note(user): the mappings should be undone in reverse order, because the
  // code that keeps "tails" up to date depends on it.
  for (int i = num_mappings_now - 1; i >= num_mappings_before; --i) {
    const int s = mapping_src_stack_[i];
    const int d = ImageOf(s);

    if (ancestor_[s] != s) loose_ends_.insert(s);
    loose_ends_.erase(d);

    ancestor_[d] = d;
    image_[s] = s;
  }
  mapping_src_stack_.resize(num_mappings_before);  // Shrink.
}

void DynamicPermutation::Reset() {
  for (const int i : mapping_src_stack_) {
    const int dst = image_[i];
    ancestor_[dst] = dst;
    image_[i] = i;
  }
  mapping_src_stack_.clear();
  mapping_src_size_stack_.clear();
  loose_ends_.clear();
}

std::unique_ptr<SparsePermutation> DynamicPermutation::CreateSparsePermutation()
    const {
  std::unique_ptr<SparsePermutation> sparse_perm(new SparsePermutation(Size()));
  int num_identity_singletons = 0;
  for (const int x : mapping_src_stack_) {
    if (tmp_mask_[x]) continue;
    // Deal with the special case of a trivial x->x cycle, which we do *not*
    // want to add to the sparse permutation.
    if (ImageOf(x) == x) {
      DCHECK_EQ(x, RootOf(x));
      ++num_identity_singletons;
      continue;
    }
    const int root = RootOf(x);
    int next = root;
    while (true) {
      sparse_perm->AddToCurrentCycle(next);
      tmp_mask_[next] = true;
      DCHECK_NE(next, ImageOf(next));
      next = ImageOf(next);
      if (next == root) break;
    }
    sparse_perm->CloseCurrentCycle();
  }
  for (const int x : mapping_src_stack_) tmp_mask_[x] = false;
  DCHECK_EQ(mapping_src_stack_.size(),
            sparse_perm->Support().size() + num_identity_singletons);
  return sparse_perm;
}

std::string DynamicPermutation::DebugString() const {
  // That's wasteful, but we don't care, DebugString() may be slow.
  return CreateSparsePermutation()->DebugString();
}

}  // namespace operations_research
