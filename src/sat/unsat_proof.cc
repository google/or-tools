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
#include "sat/unsat_proof.h"

namespace operations_research {
namespace sat {

// A node of the resolution DAG.
struct ResolutionNode {
 public:
  // Constructor for the root nodes.
  ResolutionNode()
      : is_locked_(true),
        is_problem_node_(true),
        is_marked_(false),
        ref_count_(1),
        parents_() {}

  // Constructor for the inner nodes.
  // We use a swap based constructor to avoid a copy.
  explicit ResolutionNode(std::vector<ResolutionNode*>* to_swap)
      : is_locked_(true),
        is_problem_node_(false),
        is_marked_(false),
        ref_count_(1) {
    CHECK(!to_swap->empty());
    to_swap->swap(parents_);
    for (ResolutionNode* node : parents_) {
      CHECK(node->IsLocked());
      ++(node->ref_count_);
    }
  }

  // We just check that the object was properly cleaned.
  ~ResolutionNode() { DCHECK(parents_.empty()); }

  // Returns the list of parents ResolutionNode pointer.
  const std::vector<ResolutionNode*>& parents() const { return parents_; }

  // Decrements the reference counter of this node and returns true if it
  // reaches zero. In the later case, the object ownership is transfered to the
  // caller who is free to delete it. Nodes on which DecrementReferenceCounter()
  // must be called are appended to to_decrement.
  bool DecrementReferenceCounter(std::vector<ResolutionNode*>* to_decrement) {
    CHECK_GT(ref_count_, 0);
    // Nothing to do if the reference count is still positive.
    --ref_count_;
    if (ref_count_ > 0) return false;
    for (ResolutionNode* node : parents_) {
      to_decrement->push_back(node);
    }
    parents_.clear();
    return true;
  }

  // Setter/Getter for a boolean marker.
  bool MarkAndTestIfFirstTime() {
    if (is_marked_) return false;
    is_marked_ = true;
    return true;
  }
  void ClearMark() { is_marked_ = false; }

  bool IsProblemNode() const { return is_problem_node_; }
  bool IsLocked() const { return is_locked_; }
  void Unlock() { is_locked_ = false; }

 private:
  // Indicates if this node is "locked". That means it is referenced from
  // outside the UnsatProof classes and as such it can be deleted.
  bool is_locked_ : 1;

  // Indicates if this node correspond to a problem node or not.
  bool is_problem_node_ : 1;

  // Marker used by algorithms traversing the DAG of ResolutionNode.
  bool is_marked_ : 1;

  // Number of ResolutionNodePointer pointing to this ResolutionNode.
  // This is used to implement a reference counting and delete this object when
  // the count reach 0. We do not use shared_ptr for two reasons:
  // - Its size is the one of 2 pointers which is too much.
  // - Since our nodes form a DAG which is potentially very deep, it may cause
  //   too much recursive call between the destructors.
  int32 ref_count_;

  // The clause corresponding to this Resolution node can be derived from the
  // clauses corresponding to the parents by the "resolution rule" (or
  // subsumption): (A v x) and (B v not(x)) => A v B.
  //
  // The parents are stored in order so that we start by the first parent clause
  // and then resolve it by each of the following clause in order.
  std::vector<ResolutionNode*> parents_;

  DISALLOW_COPY_AND_ASSIGN(ResolutionNode);
};

UnsatProof::~UnsatProof() {
  CHECK_EQ(num_nodes_, 0);
}

ResolutionNode* UnsatProof::CreateNewRootNode(int clause_index) {
  ++num_nodes_;
  ResolutionNode* node = new ResolutionNode();
  root_node_to_clause_index_[node] = clause_index;
  return node;
}

ResolutionNode* UnsatProof::CreateNewResolutionNode(
    std::vector<ResolutionNode*>* to_swap) {
  ++num_nodes_;
  ResolutionNode* node = new ResolutionNode(to_swap);
  CHECK(!node->parents().empty());
  return node;
}

void UnsatProof::UnlockNode(ResolutionNode* node) {
  if (node == nullptr) return;
  CHECK(node->IsLocked()) << "Node already released!";
  node->Unlock();
  node_stack_.clear();
  node_stack_.push_back(node);
  while (!node_stack_.empty()) {
    ResolutionNode* current_node = node_stack_.back();
    node_stack_.pop_back();
    if (current_node->DecrementReferenceCounter(&node_stack_)) {
      --num_nodes_;
      delete current_node;
    }
  }
}

void UnsatProof::ComputeUnsatCore(
    ResolutionNode* final_node, std::vector<int>* core) const {
  core->clear();
  to_unmark_.clear();
  node_stack_.assign(1, final_node);
  while (!node_stack_.empty()) {
    const ResolutionNode* current_node = node_stack_.back();
    node_stack_.pop_back();
    for (ResolutionNode* node : current_node->parents()) {
      if (node->MarkAndTestIfFirstTime()) {
        to_unmark_.push_back(node);
        if (node->IsProblemNode()) {
          core->push_back(root_node_to_clause_index_.find(node)->second);
        }
        if (!node->parents().empty()) {
          node_stack_.push_back(node);
        }
      }
    }
  }

  // Clean after us.
  for (ResolutionNode* node : to_unmark_) node->ClearMark();
}

}  // namespace sat
}  // namespace operations_research
