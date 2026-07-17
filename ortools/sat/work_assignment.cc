// Copyright 2010-2025 Google LLC
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

#include "ortools/sat/work_assignment.h"

#include <algorithm>
#include <cmath>
#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "absl/algorithm/container.h"
#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/strings/str_cat.h"
#include "absl/synchronization/mutex.h"
#include "absl/types/span.h"
#include "ortools/base/stl_util.h"
#include "ortools/base/types.h"
#include "ortools/sat/clause.h"
#include "ortools/sat/cp_model_mapping.h"
#include "ortools/sat/cp_model_utils.h"
#include "ortools/sat/integer.h"
#include "ortools/sat/integer_base.h"
#include "ortools/sat/integer_search.h"
#include "ortools/sat/lrat_proof_handler.h"
#include "ortools/sat/model.h"
#include "ortools/sat/restart.h"
#include "ortools/sat/sat_base.h"
#include "ortools/sat/sat_decision.h"
#include "ortools/sat/sat_parameters.pb.h"
#include "ortools/sat/sat_solver.h"
#include "ortools/sat/synchronization.h"
#include "ortools/sat/util.h"
#include "ortools/util/time_limit.h"

namespace operations_research::sat {
namespace {
const int kNumInitialRestarts = 10;

// If you build a tree by expanding the nodes with minimal depth+discrepancy,
// the number of leaves when all nodes less than a given value have been split
// follows the fibonacci sequence:
// num_leaves(0) := 1;
// num_leaves(1) := 2;
// num_leaves(n) := num_leaves(n-1) + num_leaves(n-2)
// This function returns f(n) := min({i | num_leaves(i) >= n})
int MaxAllowedDiscrepancyPlusDepth(int num_leaves) {
  int i = 0;
  int a = 1;
  int b = 2;
  while (a < num_leaves) {
    std::tie(a, b) = std::make_pair(b, a + b);
    ++i;
  }
  return i;
}

// Returns the maximum depth of any leaf in the shared tree.
// This is an upper bound that can be computed without needing a lock on the
// shared tree.
int MaxPossibleLeafDepth(const SatParameters& params) {
  const int num_leaves = params.shared_tree_open_leaves_per_worker() *
                         params.shared_tree_num_workers();
  switch (params.shared_tree_split_strategy()) {
    case SatParameters::SPLIT_STRATEGY_DISCREPANCY:
    case SatParameters::SPLIT_STRATEGY_AUTO:
      return MaxAllowedDiscrepancyPlusDepth(num_leaves) +
             params.shared_tree_balance_tolerance();
    case SatParameters::SPLIT_STRATEGY_BALANCED_TREE:
      return std::ceil(std::log2(num_leaves)) +
             params.shared_tree_balance_tolerance();
    default:
      return num_leaves;
  }
}

// Returns true if `clause` is guaranteed to be able to propagate `literal` at
// `node`.
// This is only intended for DCHECKs.
bool ReasonClauseIsValidForDebug(const SharedTreeEncoder::Node* node,
                                 Literal literal, ClausePtr clause) {
  absl::flat_hash_set<Literal> valid_literals;
  CHECK_NE(literal.Index(), kNoLiteralIndex);
  valid_literals.insert(literal);
  while (!node->shared().is_root()) {
    valid_literals.insert(node->decoded_decision().Negated());
    node = node->parent();
  }
  return absl::c_all_of(clause.GetLiterals(),
                        [&valid_literals](Literal literal) {
                          return valid_literals.contains(literal);
                        });
}

// Returns true if node `a` has a lower id than node `b`.
bool NodeIdLess(const SharedTreeEncoder::Node* a,
                const SharedTreeEncoder::Node* b) {
  return a->shared().id < b->shared().id;
}
}  // namespace

SharedTreeEncoder::Node::Node(SharedTreeEncoder* encoder, NodeId id,
                              ProtoLiteral decision,
                              std::optional<Literal> decoded_decision,
                              Node* parent)
    : decoded_decision_(decoded_decision.value_or(Literal(kNoLiteralIndex))),
      encoder_(encoder),
      parent_node_(parent),
      level_start_(this) {
  DCHECK(decoded_decision.has_value() || parent == nullptr);
  shared_.id = id;
  shared_.parent_id = parent != nullptr ? parent->shared().id : kNoNodeId;
  shared_.decision = decision;
}

SharedTreeEncoder::Node* SharedTreeEncoder::Node::GetLevelStart() {
  Node* node = this;
  while (node->shared_.is_implied) {
    Node* ancestor = node->parent_node_->level_start_;
    node->level_start_ = ancestor->level_start_;
    node = ancestor;
  }
  return node;
}

bool SharedTreeEncoder::Node::AddImplication(ProtoLiteral literal) {
  DCHECK(!shared_.is_closed());
  DCHECK(!shared_.is_implied);
  auto [it, inserted] =
      shared_.var_lower_bounds.emplace(literal.proto_var(), literal.lb());
  if (!inserted && it->second >= literal.lb()) return false;
  it->second = literal.lb();
  vars_with_new_bound_.insert(literal.proto_var());
  return true;
}

IntegerValue SharedTreeEncoder::Node::GetImpliedLowerBoundForVar(
    int var) const {
  auto it = shared_.var_lower_bounds.find(var);
  if (it != shared_.var_lower_bounds.end()) {
    return it->second;
  }
  return kMinIntegerValue;
}

void SharedTreeEncoder::Node::ExportInferredReasonClause(
    Literal literal, absl::Span<const ClausePtr> proof) {
  if (encoder_->lrat_proof_handler_ == nullptr) return;
  reason_clauses_.emplace(literal,
                          encoder_->NewInferredClause(
                              ExpectedReasonClauseLiterals(literal), proof));
}

int SharedTreeEncoder::Node::GetLevel() {
  int level = 0;
  Node* decision_node = GetLevelStart();
  while (!decision_node->shared().is_root()) {
    level++;
    decision_node = decision_node->parent()->GetLevelStart();
  }
  return level;
}

ClausePtr SharedTreeEncoder::Node::ReasonClauseOrNull(Literal literal) const {
  auto it = reason_clauses_.find(literal);
  if (it == reason_clauses_.end()) return kNullClausePtr;
  return it->second;
}

void SharedTreeEncoder::Node::SetObjectiveLowerBound(
    IntegerValue objective_lb) {
  if (shared_.objective_lb >= objective_lb) return;
  shared_.objective_lb = objective_lb;
  encoder_->ProcessNewObjectiveBound(this);
}

bool SharedTreeEncoder::Node::LiteralIsTrue(ProtoLiteral literal) {
  Node* node = this;
  while (node != nullptr) {
    node = node->GetLevelStart();
    if (node->shared().is_closed()) return false;
    if (node->shared().decision.proto_var() == literal.proto_var() &&
        node->shared().decision.lb() >= literal.lb()) {
      return true;
    }
    if (node->shared().LiteralIsImplied(literal)) return true;
    node = node->parent();
  }
  return false;
}

void SharedTreeEncoder::Node::Cleanup() {
  for (auto [literal, clause] : reason_clauses()) {
    encoder_->DeleteClause(clause);
  }
  gtl::STLClearObject(&shared_.var_lower_bounds);
  gtl::STLClearObject(&reason_clauses_);
  gtl::STLClearObject(&vars_with_new_bound_);
}

std::vector<Literal> SharedTreeEncoder::Node::NegatedDecisions() const {
  std::vector<Literal> literals;
  const Node* node = this;
  while (node != nullptr && !node->shared().is_root()) {
    literals.push_back(node->decoded_decision().Negated());
    node = node->parent()->GetLevelStart();
  }
  return literals;
}

std::vector<Literal> SharedTreeEncoder::Node::ExpectedReasonClauseLiterals(
    Literal implied) const {
  CHECK_NE(implied.Index(), kNoLiteralIndex);
  std::vector<Literal> literals = NegatedDecisions();
  literals.push_back(implied);
  return literals;
}

SharedTreeEncoder::SharedTreeEncoder(LratProofHandler* lrat_proof_handler,
                                     CpModelMapping* mapping,
                                     IntegerEncoder* integer_encoder)
    : lrat_proof_handler_(lrat_proof_handler),
      mapping_(mapping),
      integer_encoder_(integer_encoder) {}

Literal SharedTreeEncoder::Decode(ProtoLiteral literal) {
  CHECK_NE(literal, ProtoLiteral());
  if (mapping_ == nullptr) {
    // We only decode a literal with no worker mapping to produce an LRAT proof.
    const int ref = literal.proto_var();
    return Literal(BooleanVariable(PositiveRef(ref)), RefIsPositive(ref));
  }
  DCHECK_LT(literal.proto_var(), mapping_->NumProtoVariables());
  if (mapping_->IsBoolean(literal.proto_var())) {
    return mapping_->Literal(literal.proto_var());
  }
  const IntegerLiteral il = IntegerLiteral::GreaterOrEqual(
      mapping_->Integer(literal.proto_var()), literal.lb());
  return integer_encoder_->GetOrCreateAssociatedLiteral(il);
}

std::optional<ProtoLiteral> SharedTreeEncoder::Encode(Literal literal) {
  std::optional<ProtoLiteral> result = EncodeLiteral(literal);
  if (result.has_value()) return result;
  for (IntegerLiteral int_lit : integer_encoder_->GetIntegerLiterals(literal)) {
    IntegerVariable positive_var = PositiveVariable(int_lit.var);
    const int model_var =
        mapping_->GetProtoVariableFromIntegerVariable(positive_var);
    if (model_var == -1) continue;
    ProtoLiteral result{
        int_lit.var == positive_var ? model_var : NegatedRef(model_var),
        int_lit.bound};
    return result;
  }
  return std::nullopt;
}

std::optional<ProtoLiteral> SharedTreeEncoder::EncodeLiteral(Literal literal) {
  CHECK_NE(literal.Index(), kNoLiteralIndex);
  if (mapping_ == nullptr) {
    // We only encode a literal with no worker mapping to produce an LRAT proof.
    ProtoLiteral result(literal.Variable().value(), 1);
    return literal.IsPositive() ? result : result.Negated();
  }

  int model_var =
      mapping_->GetProtoVariableFromBooleanVariable(literal.Variable());
  if (model_var == -1) {
    return std::nullopt;
  }
  DCHECK(mapping_->IsBoolean(model_var));
  ProtoLiteral result{model_var, 1};
  return literal.IsPositive() ? result : result.Negated();
}

SharedTreeEncoder::Node* SharedTreeEncoder::ImportNode(
    const SharedTreeNode& node,
    const absl::flat_hash_map<Literal, ClausePtr>& importable_reason_clauses) {
  Node* decoded_node = EnsureNodeExists(node.id, node.parent_id, node.decision);
  if (!node.is_root()) {
    // Ensure that if a node has any children, it has both.
    Node* sibling = EnsureNodeExists(node.sibling_id(), node.parent_id,
                                     node.decision.Negated());
    decoded_node->set_sibling(sibling);
    sibling->set_sibling(decoded_node);

    if (decoded_node->parent()->shared().is_closed()) {
      CloseNodeInternal(decoded_node, {}, /*parent_is_closed=*/true);
      CloseNodeInternal(sibling, {}, /*parent_is_closed=*/true);
    }
  }
  UpdateNode(node, importable_reason_clauses);
  ProcessNodeChanges();
  return decoded_node;
}

void SharedTreeEncoder::SplitNode(NodeId parent, ProtoLiteral decision,
                                  NodeId first_child_id) {
  ImportNode({.id = first_child_id, .parent_id = parent, .decision = decision});
}

void SharedTreeEncoder::CloseNode(NodeId node_id,
                                  absl::Span<const ClausePtr> proof) {
  Node* node = GetNode(node_id);
  CloseNodeInternal(node, proof);
  ProcessNodeChanges();
  CHECK(GetNode(node_id)->shared().is_closed());
  if (lrat_proof_handler_ != nullptr && !node->shared().is_root()) {
    CHECK(node->parent()->GetLevelStart()->shared().is_closed() ||
          node->parent()->GetLevelStart()->reason_clauses().contains(
              node->decoded_decision().Negated()));
  }
}

void SharedTreeEncoder::CloseNodeInternal(Node* node,
                                          absl::Span<const ClausePtr> proof,
                                          bool parent_is_closed) {
  if (node->shared().is_closed()) return;

  ClausePtr closing_clause = kNullClausePtr;
  if (!parent_is_closed) {
    // If any node in a level is closed, the whole level is closed, so we close
    // the start of the level unless we are just closing children of an already
    // closed node. If `node` isn't implied, then level_start == node, and this
    // block just infers an appropriate closing clause for `node` from `proof`.
    Node* level_start = node->GetLevelStart();
    if (lrat_proof_handler_ != nullptr) {
      // If node is implied, this will add a clause propagating
      // node->decoded_decision to `tmp_proof_`.
      SetProofToPropagateImpliedNodes(node);
      // `proof` must be sufficient to prove the decisions up to `node` lead to
      // conflict, if we append these clauses to `tmp_proof_`, we can infer that
      // the non-implied decisions up to and including `level_start` must lead
      // to a conflict.
      for (const ClausePtr proof_clause : proof) {
        CHECK_NE(proof_clause, kNullClausePtr);
        tmp_proof_.push_back(proof_clause);
      }
      if (node->shared().is_implied) {
        DCHECK_NE(level_start->ReasonClauseOrNull(node->decoded_decision()),
                  kNullClausePtr);
        DCHECK(absl::c_contains(tmp_proof_, level_start->ReasonClauseOrNull(
                                                node->decoded_decision())));
      }
      closing_clause =
          NewInferredClause(level_start->NegatedDecisions(), tmp_proof_);
      tmp_proof_.clear();
    }
    node = level_start;
  }
  CHECK(!node->shared().is_implied || parent_is_closed);
  closed_leaves_ += node->is_leaf();
  node->shared_.objective_lb = kMaxIntegerValue + 1;
  if (!node->shared().is_root() && !parent_is_closed) {
    ProcessImpliedNode(node->sibling(), closing_clause);
  } else {
    DeleteClause(closing_clause);
  }
  // Make sure the children of a closed node are closed.
  if (!node->is_leaf()) {
    to_close_.push_back(node->children_[0]);
    to_close_.push_back(node->children_[1]);
  }
  node->Cleanup();
}

void SharedTreeEncoder::ProcessImpliedNode(Node* node,
                                           ClausePtr decision_reason_clause) {
  DCHECK(!node->shared().is_implied);
  DCHECK(!node->shared().is_root());
  DCHECK(!node->shared().is_closed());
  node->shared_.is_implied = true;
  Node* new_level_start = node->GetLevelStart();
  CHECK_NE(new_level_start, node);
  CHECK(!new_level_start->shared().is_closed());
  SetProofToPropagateImpliedNodes(node->parent_node_);
  if (new_level_start->AddImplication(node->shared().decision) &&
      lrat_proof_handler_ != nullptr) {
    tmp_proof_.push_back(decision_reason_clause);
    new_level_start->ExportInferredReasonClause(node->decoded_decision(),
                                                tmp_proof_);
    tmp_proof_.pop_back();
  }
  DeleteClause(decision_reason_clause);
  tmp_proof_.push_back(
      new_level_start->ReasonClauseOrNull(node->decoded_decision()));
  // The decision must be implied at the start of the level.
  CHECK(new_level_start->shared().LiteralIsImplied(node->shared().decision));
  // Update variable bounds.
  for (auto& [var, lb] : node->shared_.var_lower_bounds) {
    new_level_start->AddImplication(ProtoLiteral(var, lb));
  }
  MoveReasonClausesToLevelStart(node);
  tmp_proof_.clear();
  node->Cleanup();
  if (node->shared().objective_lb > new_level_start->shared().objective_lb) {
    new_level_start->shared_.objective_lb = node->shared().objective_lb;
    ProcessNewObjectiveBound(new_level_start);
  }
}

void SharedTreeEncoder::ProcessNewObjectiveBound(Node* node) {
  while (!node->shared().is_root()) {
    Node* prev_level_start = node->parent()->GetLevelStart();
    if (node->shared().objective_lb <=
        prev_level_start->shared().objective_lb) {
      return;
    }
    const Node* sibling = node->sibling();
    prev_level_start->shared_.objective_lb =
        std::min(sibling->shared().objective_lb, node->shared().objective_lb);
    node = prev_level_start;
  }
}

SharedTreeEncoder::Node* SharedTreeEncoder::EnsureNodeExists(
    NodeId id, NodeId parent_id, ProtoLiteral decision) {
  auto it = nodes_by_id_.emplace(id, nullptr).first;
  if (it->second != nullptr) return it->second;

  Node* parent = parent_id == kNoNodeId ? nullptr : GetNode(parent_id);
  const std::optional<Literal> decoded_decision =
      (parent != nullptr) ? std::make_optional(Decode(decision)) : std::nullopt;
  nodes_.emplace_back(this, id, decision, decoded_decision, parent);
  Node* node = &nodes_.back();
  it->second = node;

  if (!node->shared().is_root()) {
    parent->set_child(node);
    // Handle the edge case of creating the child of a closed node.
    if (parent->shared().is_closed()) {
      closed_leaves_ -= (node->shared().id.value() & 1);
      to_close_.push_back(node);
    } else {
      node->shared_.objective_lb = parent->shared().objective_lb;
    }
  }
  return node;
}

void SharedTreeEncoder::UpdateNode(
    const SharedTreeNode& other,
    const absl::flat_hash_map<Literal, ClausePtr>& importable_reason_clauses) {
  Node* node = GetNode(other.id);
  if (node->shared().is_closed()) return;
  if (other.is_closed()) {
    Node* prev_level_start =
        node->shared().is_root()
            ? nullptr
            : GetNode(node->shared().parent_id)->GetLevelStart();
    ClausePtr clause = node->shared().is_root()
                           ? NewImportedClause({})
                           : prev_level_start->ReasonClauseOrNull(
                                 node->decoded_decision().Negated());
    if (lrat_proof_handler_ != nullptr && prev_level_start != nullptr) {
      CHECK_NE(clause, kNullClausePtr);
    }
    CloseNodeInternal(node, {clause});
    return;
  }

  if (other.is_implied && !node->shared().is_implied) {
    Node* sibling = node->sibling();
    // We should already have imported all clauses from the start of the
    // level, find the right one.
    ClausePtr clause = node->parent_node_->GetLevelStart()->ReasonClauseOrNull(
        node->decoded_decision());
    if (lrat_proof_handler_ != nullptr) {
      CHECK_NE(clause, kNullClausePtr);
    }
    CloseNodeInternal(sibling, {clause});
  }
  if (node->shared().objective_lb < other.objective_lb) {
    node->shared_.objective_lb = other.objective_lb;
    ProcessNewObjectiveBound(node);
  }
  if (other.var_lower_bounds.empty()) return;
  Node* level_start = node->GetLevelStart();
  SetProofToPropagateImpliedNodes(node);
  for (const auto& [var, bound] : other.var_lower_bounds) {
    const ProtoLiteral implied = ProtoLiteral(var, bound);
    if (level_start->AddImplication(implied) &&
        lrat_proof_handler_ != nullptr) {
      const Literal decoded = Decode(implied);
      ClausePtr clause = NewImportedClause(
          importable_reason_clauses.at(decoded).GetLiterals());
      if (node->shared().id != level_start->shared().id) {
        tmp_proof_.push_back(clause);
        clause = NewInferredClause(
            level_start->ExpectedReasonClauseLiterals(decoded), tmp_proof_);
        DeleteClause(tmp_proof_.back());
        tmp_proof_.pop_back();
      }
      level_start->reason_clauses_.emplace(decoded, clause);
    }
  }
}

void SharedTreeEncoder::ResolveBoundsOnPath(Node* leaf) {
  if (leaf == nullptr) return;
  Node* node = leaf->GetLevelStart();
  while (!node->shared().is_root()) {
    Node* sibling = node->sibling();
    Node* parent_level_start = node->parent()->GetLevelStart();
    if (!node->shared().is_closed()) {
      for (int var : node->vars_with_new_bound_) {
        const IntegerValue resolved_bound =
            std::min(node->GetImpliedLowerBoundForVar(var),
                     sibling->GetImpliedLowerBoundForVar(var));
        if (resolved_bound == kMinIntegerValue) continue;
        ProtoLiteral implied = ProtoLiteral(var, resolved_bound);
        // Note: this call updates parent_level_start->vars_with_new_bound_, so
        // we will resolve the new bounds further up the tree on the next
        // iteration.
        if (parent_level_start->AddImplication(implied) &&
            lrat_proof_handler_ != nullptr) {
          const Literal decoded = Decode(implied);
          SetProofToPropagateImpliedNodes(node->parent());
          tmp_proof_.push_back(node->ReasonClauseOrNull(decoded));
          tmp_proof_.push_back(node->sibling()->ReasonClauseOrNull(decoded));
          parent_level_start->ExportInferredReasonClause(decoded, tmp_proof_);
        }
      }
    }
    node->vars_with_new_bound_.clear();
    node = parent_level_start;
  }
}

void SharedTreeEncoder::SyncNodesOnPath(NodeId leaf_id,
                                        SharedTreeEncoder& worker_tree) {
  if (leaf_id == kNoNodeId) return;
  DCHECK(CheckInvariants());
  DCHECK(worker_tree.CheckInvariants());
  to_update_.clear();
  Node* leaf = GetNode(leaf_id);
  {
    Node* node = leaf;
    while (node != nullptr) {
      to_update_.push_back(node);
      node = node->parent();
    }
    absl::c_reverse(to_update_);
  }
  // Tricky: We must import all nodes into one tree first.
  // If instead each node is imported into `this` and `worker_tree` before
  // moving on to its child, we will miss implications when a node becomes
  // implied, as implications are moved to the parent which was imported
  // earlier.
  for (Node* node : to_update_) {
    Node* worker_node = worker_tree.nodes_by_id_[node->shared().id];
    if (worker_node == nullptr) break;
    ImportNode(worker_node->shared(), worker_node->reason_clauses());
    if (node->shared().is_closed()) break;
  }
  ResolveBoundsOnPath(leaf);
  for (const Node* node : to_update_) {
    Node* worker_node =
        worker_tree.ImportNode(node->shared(), node->reason_clauses());
    worker_node->vars_with_new_bound_.clear();
  }
  to_update_.clear();
}

void SharedTreeEncoder::Clear() {
  if (lrat_proof_handler_ != nullptr) {
    for (auto& node : nodes_) {
      node.Cleanup();
    }
  }
  nodes_.clear();
  nodes_by_id_.clear();
  closed_leaves_ = 0;
}

std::vector<SharedTreeEncoder::Node*> SharedTreeEncoder::GetLevelStartNodes(
    NodeId leaf_id) {
  std::vector<Node*> nodes;
  if (leaf_id == kNoNodeId) return nodes;
  Node* node = GetNode(leaf_id);
  while (node != nullptr) {
    Node* level_start = node->GetLevelStart();
    nodes.push_back(level_start);
    node = level_start->parent();
  }
  absl::c_reverse(nodes);
  return nodes;
}

void SharedTreeEncoder::SetProofToPropagateImpliedNodes(Node* node) {
  if (lrat_proof_handler_ == nullptr) return;
  tmp_proof_.clear();
  // Every implied decoded_decision must have a clause stored at the start of
  // its level, collect all of these into tmp_proof_.
  while (node != nullptr) {
    Node* level_start = node->GetLevelStart();
    while (node != level_start) {
      DCHECK(node->shared().is_implied);
      tmp_proof_.push_back(
          level_start->reason_clauses().at(node->decoded_decision()));
      DCHECK_NE(tmp_proof_.back(), kNullClausePtr);
      node = node->parent();
    }
    node = level_start->parent();
  }
  // Ensure clauses are stored in an order where they will propagate
  // (implications from nodes closer to the root first).
  absl::c_reverse(tmp_proof_);
}

void SharedTreeEncoder::MoveReasonClausesToLevelStart(Node* implied_node) {
  if (lrat_proof_handler_ == nullptr) return;
  SetProofToPropagateImpliedNodes(implied_node);
  Node* const new_level_start = implied_node->GetLevelStart();
  for (const auto& [implied, clause] : implied_node->reason_clauses()) {
    auto [it, inserted] =
        new_level_start->reason_clauses_.emplace(implied, kNullClausePtr);
    if (inserted) {
      tmp_proof_.push_back(clause);
      it->second = NewInferredClause(
          new_level_start->ExpectedReasonClauseLiterals(implied), tmp_proof_);
      tmp_proof_.pop_back();
    }
  }
}

void SharedTreeEncoder::DeleteClause(ClausePtr clause) {
  if (clause == kNullClausePtr) return;
  DCHECK_NE(lrat_proof_handler_, nullptr);
  // Binary clauses could get deleted by other code, so NewImportedClause and
  // NewInferredClause use a fake SatClause instead for them to guarantee they
  // have a unique ID that won't be deleted.
  DCHECK_NE(clause.GetType(), ClausePtr::kBinaryClause);
  // Don't delete unit or empty clauses as these may be referenced at level 0 on
  // the trail, and thus must remain in the proof past a call to Clear.
  if (clause.GetType() != ClausePtr::kSatClause) return;
  lrat_proof_handler_->DeleteClause(clause);
}

ClausePtr SharedTreeEncoder::NewImportedClause(
    absl::Span<const Literal> literals) {
  if (lrat_proof_handler_ == nullptr) return kNullClausePtr;
  // Tricky: To ensure binary clauses are not deleted while we are still using
  // them, we make sure we use an allocated SatClause for binary and larger
  // clauses.
  ClausePtr clause =
      literals.size() <= 1 ? NewClausePtr(literals) : ClausePtr(literals);
  lrat_proof_handler_->AddImportedClause(clause);
  return clause;
}

ClausePtr SharedTreeEncoder::NewInferredClause(
    absl::Span<const Literal> literals, absl::Span<const ClausePtr> proof) {
  if (lrat_proof_handler_ == nullptr) return kNullClausePtr;
  // Tricky: To ensure binary clauses are not deleted while we are still using
  // them, we make sure we use an allocated SatClause for binary and larger
  // clauses.
  ClausePtr clause =
      literals.size() <= 1 ? NewClausePtr(literals) : ClausePtr(literals);
  lrat_proof_handler_->AddInferredClause(clause, proof, /*exported=*/true);
  return clause;
}

void SharedTreeEncoder::NormalizeReasonClauses(Node* node) {
  if (lrat_proof_handler_ == nullptr) return;
  const int level = node->GetLevel();
  SetProofToPropagateImpliedNodes(node);
  std::vector<Literal> reason_literals = node->NegatedDecisions();
  for (auto& [literal, clause] : node->reason_clauses_) {
    if (clause.GetLiterals().size() > level + 1) {
      tmp_proof_.push_back(clause);
      reason_literals.push_back(literal);
      clause = NewInferredClause(reason_literals, tmp_proof_);
      reason_literals.pop_back();
      DeleteClause(tmp_proof_.back());
      tmp_proof_.pop_back();
    }
  }
  DCHECK(CheckInvariants());
}

void SharedTreeEncoder::ProcessNodeChanges() {
  while (!to_close_.empty()) {
    Node* node = to_close_.back();
    to_close_.pop_back();
    CloseNodeInternal(node, {}, /*parent_is_closed=*/true);
  }
  DCHECK(CheckInvariants());
}

bool SharedTreeEncoder::CheckInvariants() {
  CHECK(to_close_.empty());
  int closed_leaves = 0;
  int total_leaves = 0;
  for (const auto& node_ref : nodes_) {
    const Node* node = &node_ref;
    const NodeId id = node->shared().id;
    if (node->is_leaf()) {
      ++total_leaves;
      closed_leaves += node->shared().is_closed();
    }
    CHECK_EQ(id, node->shared().id);
    if (lrat_proof_handler_ != nullptr) {
      for (const auto& [var, bound] : node->shared().var_lower_bounds) {
        const Literal implied = Decode(ProtoLiteral(var, bound));
        CHECK(node->reason_clauses().contains(implied));
        CHECK(ReasonClauseIsValidForDebug(node, implied,
                                          node->reason_clauses().at(implied)));
      }
    }
    if (node->shared().is_root()) continue;
    CHECK_EQ(node->sibling(), GetNode(node->shared().sibling_id()));
    const Node* parent = node->parent();
    CHECK_EQ(parent->child(0)->shared().id,
             std::min(node->shared().id, node->shared().sibling_id()));
    if (node->shared().is_closed()) {
      // Check this node's sibling is closed or implied.
      Node* sibling_node = node->sibling();
      if (sibling_node->shared().is_closed()) {
        // If both children are closed, the parent must be closed.
        CHECK(parent->shared().is_closed());
      } else {
        Node* level_start = sibling_node->GetLevelStart();
        CHECK(sibling_node->shared().is_implied);
        if (!level_start->shared().is_root()) {
          CHECK(level_start->shared().LiteralIsImplied(
              sibling_node->shared().decision));
        }
        if (lrat_proof_handler_ != nullptr) {
          CHECK(level_start->reason_clauses().contains(
              sibling_node->decoded_decision()));
          CHECK(ReasonClauseIsValidForDebug(
              level_start, sibling_node->decoded_decision(),
              level_start->reason_clauses().at(
                  sibling_node->decoded_decision())));
        }
      }
    } else {
      // If a node is not closed, its parent must not be closed.
      CHECK(!parent->shared().is_closed());
    }
  }
  CHECK_EQ(closed_leaves, ClosedLeaves());
  CHECK_EQ(total_leaves, TotalLeaves());
  return true;
}

SharedTreeManager::SharedTreeManager(Model* model)
    : params_(*model->GetOrCreate<SatParameters>()),
      num_workers_(std::max(0, params_.shared_tree_num_workers())),
      max_path_depth_(MaxPossibleLeafDepth(params_)),
      shared_response_manager_(model->GetOrCreate<SharedResponseManager>()),
      lrat_proof_handler_(LratProofHandler::MaybeCreate(
          params_, model->GetOrCreate<SharedLratProofStatus>(),
          model->GetOrCreate<SharedStatistics>())),
      tree_(lrat_proof_handler_.get()),
      max_nodes_(params_.shared_tree_max_nodes_per_worker() >=
                         kint32max / std::max(num_workers_, 1)
                     ? kint32max
                     : num_workers_ *
                           params_.shared_tree_max_nodes_per_worker()) {
  // Create the root node with a fake decision.
  tree_.ImportNode({
      .id = root_,
      .decision = ProtoLiteral(),
      .objective_lb = shared_response_manager_->GetInnerObjectiveLowerBound(),
  });
  unassigned_leaves_.emplace_back(root_, std::vector<ProtoLiteral>());
}

int SharedTreeManager::NumNodes() const {
  absl::MutexLock l(mu_);
  return tree_.Size();
}

bool SharedTreeManager::SyncTree(NodeId leaf_id, SharedTreeEncoder& encoder) {
  absl::MutexLock mutex_lock(mu_);
  const int num_initially_closed = tree_.ClosedLeaves();
  const IntegerValue old_root_lb = tree_.GetNode(root_)->shared().objective_lb;
  if (!IsValid(leaf_id)) {
    encoder.Clear();
  } else {
    tree_.SyncNodesOnPath(leaf_id, encoder);
  }
  if (tree_.ClosedLeaves() > num_initially_closed) {
    shared_response_manager_->LogMessageWithThrottling(
        "Tree",
        absl::StrCat("closed:", tree_.ClosedLeaves(), "/", tree_.TotalLeaves(),
                     " unassigned:", unassigned_leaves_.size(),
                     " restarts:", num_restarts_));
  }
  if (tree_.OpenLeaves() == 0) {
    // Ensure the worker imports the empty clause, even if the leaf was invalid.
    encoder.ImportNode(tree_.GetNode(root_)->shared());
    shared_response_manager_->NotifyThatImprovingProblemIsInfeasible(
        ShortStatus());
    return false;
  }

  // Update the global objective lower bound if the root's bound has improved.
  const IntegerValue new_root_lb = tree_.GetNode(root_)->shared().objective_lb;
  if (new_root_lb > old_root_lb) {
    shared_response_manager_->UpdateInnerObjectiveBounds(
        ShortStatus(), new_root_lb, kMaxIntegerValue);
  }

  // Restart after processing updates - we might learn a new objective bound.
  // Do initial restarts once each worker has had the chance to be assigned a
  // leaf.
  if (num_leaves_assigned_since_restart_ >= num_workers_ &&
      num_restarts_ < kNumInitialRestarts) {
    RestartLockHeld();
    encoder.Clear();
  }
  return true;
}

NodeId SharedTreeManager::TrySplitTree(NodeId parent,
                                       absl::Span<const ProtoLiteral> decisions,
                                       SharedTreeEncoder& encoder) {
  const int max_splits =
      MaxPossibleLeafDepth(params_) - encoder.GetNode(parent)->GetLevel();
  decisions = decisions.subspan(0, std::max(0, max_splits));
  if (decisions.empty()) return parent;
  absl::MutexLock l(mu_);
  for (int i = 0; i < decisions.size(); ++i) {
    NodeId child = TrySplitTreeLockHeld(parent, decisions[i], encoder);
    if (child == kNoNodeId) break;
    parent = child;
  }
  return parent;
}

NodeId SharedTreeManager::TrySplitTreeLockHeld(NodeId parent,
                                               ProtoLiteral decision,
                                               SharedTreeEncoder& encoder) {
  if (!IsValid(parent)) {
    VLOG(2) << "Invalid parent node";
    return kNoNodeId;
  }
  if (tree_.OpenLeaves() == 0) {
    VLOG(2) << "Problem is closed";
    return kNoNodeId;
  }
  if (tree_.OpenLeaves() >=
      params_.shared_tree_open_leaves_per_worker() * num_workers_) {
    VLOG(2) << "Enough splits for now";
    return kNoNodeId;
  }
  if (tree_.Size() >= max_nodes_ - 1) {
    VLOG(2) << "Too many nodes to accept split";
    return kNoNodeId;
  }

  SharedTreeEncoder::Node* parent_node = tree_.GetNode(parent);
  if (parent_node->shared().is_closed()) {
    VLOG(2) << "Cannot split closed node";
    return kNoNodeId;
  }
  if (!parent_node->is_leaf()) {
    VLOG(2) << "Cannot resplit previously split node";
    return kNoNodeId;
  }
  int level = 0;
  int discrepancy = 0;
  const SharedTreeEncoder::Node* ancestor = parent_node->GetLevelStart();
  while (!ancestor->shared().is_root()) {
    level++;
    discrepancy += ancestor->shared().id.value() % 2;
    ancestor = ancestor->parent()->GetLevelStart();
  }
  if (params_.shared_tree_split_strategy() ==
          SatParameters::SPLIT_STRATEGY_DISCREPANCY ||
      params_.shared_tree_split_strategy() ==
          SatParameters::SPLIT_STRATEGY_AUTO) {
    // TODO(user): Need to write up the shape this creates.
    // This rule will allow twice as many leaves in the preferred subtree.
    if (discrepancy + level >= max_path_depth_) {
      VLOG(2) << "Too high discrepancy to accept split";
      return kNoNodeId;
    }
  } else if (params_.shared_tree_split_strategy() ==
             SatParameters::SPLIT_STRATEGY_OBJECTIVE_LB) {
    if (parent_node->shared().objective_lb >
        tree_.GetNode(root_)->shared().objective_lb) {
      VLOG(2) << "Can only split nodes with minimum objective lb";
      return kNoNodeId;
    }
  }
  VLOG_EVERY_N(2, 10) << unassigned_leaves_.size() << " unassigned leaves, "
                      << tree_.Size() << " subtrees, " << tree_.OpenLeaves()
                      << " open leaves";
  tree_.SplitNode(parent, decision, next_node_id_);
  encoder.SplitNode(parent, decision, next_node_id_);
  unassigned_leaves_.emplace_back(next_node_id_ + 1,
                                  std::vector<ProtoLiteral>());
  next_node_id_ += 2;
  return next_node_id_ - 2;
}

NodeId SharedTreeManager::ReplaceTree(NodeId old_leaf_id,
                                      SharedTreeEncoder& encoder,
                                      std::vector<ProtoLiteral>& phase) {
  absl::MutexLock mutex_lock(mu_);
  if (IsValid(old_leaf_id)) {
    const SharedTreeEncoder::Node* old_leaf = tree_.GetNode(old_leaf_id);
    if (!old_leaf->shared().is_closed() && old_leaf->is_leaf()) {
      VLOG(2) << "Returning leaf to be replaced";
      unassigned_leaves_.emplace_back(old_leaf_id, std::move(phase));
    }
  }
  encoder.Clear();
  while (!unassigned_leaves_.empty()) {
    auto [new_leaf, new_phase] = std::move(unassigned_leaves_.front());
    unassigned_leaves_.pop_front();
    const SharedTreeEncoder::Node* leaf = tree_.GetNode(new_leaf);
    if (leaf->shared().is_closed()) continue;
    DCHECK(leaf->is_leaf());
    num_leaves_assigned_since_restart_ += 1;
    tree_.SyncNodesOnPath(new_leaf, encoder);
    phase = std::move(new_phase);
    return new_leaf;
  }
  VLOG(2) << "No unassigned leaves available";
  return kNoNodeId;
}

void SharedTreeManager::RestartLockHeld() {
  const IntegerValue root_lb = tree_.GetNode(root_)->shared().objective_lb;
  root_ = next_node_id_ + 1;
  CHECK_EQ(root_ % 2, 1);
  next_node_id_ = root_ + 1;
  tree_.Clear();
  tree_.ImportNode(
      {.id = root_, .parent_id = kNoNodeId, .objective_lb = root_lb});
  unassigned_leaves_.clear();
  unassigned_leaves_.emplace_back(root_, std::vector<ProtoLiteral>());
  num_restarts_ += 1;
  num_leaves_assigned_since_restart_ = 0;
}

void SharedTreeManager::CloseLratProof() {
  absl::MutexLock l(mu_);
  if (lrat_proof_handler_ != nullptr) {
    RestartLockHeld();  // This deletes all remaining LRAT clauses.
    lrat_proof_handler_->Close(/*model_is_unsat=*/false);
    lrat_proof_handler_.reset();
  }
}

std::string SharedTreeManager::ShortStatus() const {
  return absl::StrCat("shared_tree_manager(r=", num_restarts_,
                      " l=", tree_.OpenLeaves(), "/", tree_.TotalLeaves(), ")");
}

SharedTreeWorker::SharedTreeWorker(Model* model)
    : parameters_(model->GetOrCreate<SatParameters>()),
      shared_response_(model->GetOrCreate<SharedResponseManager>()),
      time_limit_(model->GetOrCreate<TimeLimit>()),
      manager_(model->GetOrCreate<SharedTreeManager>()),
      mapping_(model->GetOrCreate<CpModelMapping>()),
      sat_solver_(model->GetOrCreate<SatSolver>()),
      trail_(model->GetOrCreate<Trail>()),
      binary_propagator_(model->GetOrCreate<BinaryImplicationGraph>()),
      clause_manager_(model->GetOrCreate<ClauseManager>()),
      lrat_proof_handler_(model->Mutable<LratProofHandler>()),
      integer_trail_(model->GetOrCreate<IntegerTrail>()),
      encoder_(model->GetOrCreate<IntegerEncoder>()),
      objective_(model->Get<ObjectiveDefinition>()),
      random_(*model->GetOrCreate<ModelRandomGenerator>()),
      helper_(model->GetOrCreate<IntegerSearchHelper>()),
      heuristics_(model->GetOrCreate<SearchHeuristics>()),
      decision_policy_(model->GetOrCreate<SatDecisionPolicy>()),
      restart_policy_(model->GetOrCreate<RestartPolicy>()),
      reversible_int_repository_(model->GetOrCreate<RevIntRepository>()),
      tree_(lrat_proof_handler_, mapping_, encoder_),
      assigned_tree_lbds_(/*window_size=*/8) {}

void SharedTreeWorker::MaybeProposeSplits() {
  if (assigned_nodes_.empty()) return;
  if (sat_solver_->CurrentDecisionLevel() <= sat_solver_->AssumptionLevel()) {
    return;
  }
  if (time_limit_->GetElapsedDeterministicTime() <= next_split_dtime_) {
    return;
  }
  next_split_dtime_ = time_limit_->GetElapsedDeterministicTime() +
                      parameters_->shared_tree_split_min_dtime();
  const int max_splits = manager_->MaxPathDepth() - assigned_nodes_.size() + 1;
  for (int i = sat_solver_->AssumptionLevel();
       i < sat_solver_->CurrentDecisionLevel(); ++i) {
    const Literal split_decision = trail_->Decisions()[i].literal;
    const std::optional<ProtoLiteral> encoded = tree_.Encode(split_decision);
    // If we can't encode a decision we can't split on it.
    if (!encoded.has_value()) continue;
    // TODO(user): This should be impossible, investigate.
    SharedTreeEncoder::Node* leaf_node = tree_.GetNode(leaf_id_);
    if (leaf_node->LiteralIsTrue(*encoded)) continue;
    if (leaf_node->LiteralIsTrue(encoded->Negated())) continue;
    tmp_splits_.push_back(*encoded);
    if (tmp_splits_.size() >= max_splits) break;
  }
  const NodeId new_leaf_id =
      manager_->TrySplitTree(leaf_id_, tmp_splits_, tree_);
  tmp_splits_.clear();
  if (new_leaf_id != leaf_id_) {
    ResetAndEnqueueAssumptions(new_leaf_id);
  }
}

bool SharedTreeWorker::ShouldReplaceSubtree() {
  // If we have no assignment, try to get one.
  if (tree_.Size() == 0) return true;
  if (assigned_nodes_.empty() || assigned_nodes_.back()->shared().is_closed()) {
    return true;
  }

  if (restart_policy_->NumRestarts() <
          parameters_->shared_tree_worker_min_restarts_per_subtree() ||
      time_limit_->GetElapsedDeterministicTime() <
          earliest_replacement_dtime_) {
    return false;
  }
  return assigned_tree_lbds_.WindowAverage() <
         restart_policy_->LbdAverageSinceReset();
}

bool SharedTreeWorker::SyncWithSharedTree() {
  CHECK_EQ(trail_->CurrentDecisionLevel(), 0);
  if (!manager_->SyncTree(leaf_id_, tree_)) {
    sat_solver_->NotifyThatModelIsUnsat();
    return false;
  }
  NodeId new_leaf_id = leaf_id_;
  if (ShouldReplaceSubtree()) {
    ++num_trees_;
    VLOG(2) << parameters_->name() << " acquiring tree #" << num_trees_
            << " after " << restart_policy_->NumRestarts() << " restarts"
            << " target: " << assigned_tree_lbds_.WindowAverage()
            << " lbd: " << restart_policy_->LbdAverageSinceReset();
    if (parameters_->shared_tree_worker_enable_phase_sharing() &&
        // Only save the phase if we've done a non-trivial amount of work on
        // this subtree.
        FinishedMinRestarts() &&
        !decision_policy_->GetBestPartialAssignment().empty()) {
      phase_.clear();
      for (Literal lit : decision_policy_->GetBestPartialAssignment()) {
        if (phase_.size() > kMaxPhaseSize) break;
        // Skip anything assigned at level 0.
        if (trail_->Assignment().LiteralIsAssigned(lit)) continue;
        // If `lit` was last assigned at a shared level, it is implied in the
        // tree, no need to share its phase.
        if (trail_->Info(lit.Variable()).level < assigned_nodes_.size()) {
          continue;
        }
        // Only set the phase for booleans to avoid creating literals on other
        // workers.
        auto encoded = tree_.EncodeLiteral(lit);
        if (!encoded.has_value()) continue;
        phase_.push_back(*encoded);
      }
    }
    new_leaf_id = manager_->ReplaceTree(leaf_id_, tree_, phase_);
    assigned_tree_lbds_.Add(restart_policy_->LbdAverageSinceReset());
    restart_policy_->Reset();
    earliest_replacement_dtime_ = 0;
    if (new_leaf_id != kNoNodeId) {
      next_split_dtime_ = time_limit_->GetElapsedDeterministicTime() +
                          parameters_->shared_tree_split_min_dtime();
    }
    if (parameters_->shared_tree_worker_enable_phase_sharing()) {
      VLOG(2) << "Importing phase of length: " << phase_.size();
      decision_policy_->ClearBestPartialAssignment();
      for (const ProtoLiteral& lit : phase_) {
        decision_policy_->SetTargetPolarityIfUnassigned(tree_.Decode(lit));
      }
      decision_policy_->ResetActivitiesToFollowBestPartialAssignment();
      // This seems bizzare after just setting the best partial assignment,
      // but this makes phase sharing work even when there is no stable phase
      // in the restart strategy, and makes no real difference if there is,
      // since the first dive will still try to follow this assignment until
      // the first conflict regardless of the restart strategy.
      decision_policy_->ClearBestPartialAssignment();
    }
  }
  // If we commit to this subtree, keep it for at least 1s of dtime.
  // This allows us to replace obviously bad subtrees quickly, and not replace
  // too frequently overall.
  if (FinishedMinRestarts() && earliest_replacement_dtime_ >=
                                   time_limit_->GetElapsedDeterministicTime()) {
    earliest_replacement_dtime_ =
        time_limit_->GetElapsedDeterministicTime() + 1;
    // Treat this as reassigning the same tree.
    assigned_tree_lbds_.Add(restart_policy_->LbdAverageSinceReset());
  }
  DCHECK(tree_.CheckInvariants());
  return ResetAndEnqueueAssumptions(new_leaf_id);
}

SatSolver::Status SharedTreeWorker::Search(
    const std::function<void()>& feasible_solution_observer) {
  if (!sat_solver_->ResetToLevelZero()) return sat_solver_->UnsatStatus();
  const bool has_objective =
      objective_ != nullptr && objective_->objective_var != kNoIntegerVariable;
  while (!time_limit_->LimitReached()) {
    if (LeafIsClosed() ||
        heuristics_->restart_policies[heuristics_->policy_index]()) {
      heuristics_->policy_index = restart_policy_->NumRestarts() %
                                  heuristics_->decision_policies.size();
      sat_solver_->Backtrack(0);
    }
    if (sat_solver_->CurrentDecisionLevel() == 0 && !SyncWithSharedTree()) {
      return SatSolver::INFEASIBLE;
    }
    // Re-sync with the shared tree if the assigned subtree is closed.
    if (LeafIsClosed()) continue;

    if (sat_solver_->CurrentDecisionLevel() <= sat_solver_->AssumptionLevel()) {
      ExportNewImplications();
    }

    if (!helper_->BeforeTakingDecision() && !CloseNodeAfterConflict()) {
      return sat_solver_->UnsatStatus();
    }
    const auto& decision_policy =
        heuristics_->decision_policies[heuristics_->policy_index];
    LiteralIndex decision_index;
    if (!helper_->GetDecision(decision_policy, &decision_index)) continue;

    if (time_limit_->LimitReached()) return SatSolver::LIMIT_REACHED;
    if (decision_index == kNoLiteralIndex) {
      feasible_solution_observer();
      if (!has_objective) return SatSolver::FEASIBLE;
      const IntegerValue objective =
          integer_trail_->LowerBound(objective_->objective_var);
      sat_solver_->Backtrack(0);
      if (!integer_trail_->Enqueue(
              IntegerLiteral::LowerOrEqual(objective_->objective_var,
                                           objective - 1),
              {}, {})) {
        return SatSolver::INFEASIBLE;
      }
      if (!sat_solver_->FinishPropagation()) return sat_solver_->UnsatStatus();
      continue;
    }
    const Literal decision(decision_index);
    if (!helper_->TakeDecision(decision) && !CloseNodeAfterConflict()) {
      return sat_solver_->UnsatStatus();
    }
    MaybeProposeSplits();
  }

  return SatSolver::LIMIT_REACHED;
}

SharedTreeEncoder::Node* SharedTreeWorker::FixedByOrNull(Literal literal) {
  if (assigned_nodes_.empty()) return nullptr;
  if (trail_->AssignmentLevel(literal) == 0 &&
      trail_->Assignment().LiteralIsTrue(literal)) {
    return assigned_nodes_.front();
  }
  if (literal.Index() >= fixed_by_.size()) return nullptr;
  SharedTreeEncoder::Node* node = fixed_by_[literal.Index()];
  if (node == nullptr) return nullptr;
  return node->GetLevelStart();
}

void SharedTreeWorker::ExportNewImplications() {
  fixed_by_.resize(2 * sat_solver_->NumVariables(), nullptr);
  if (!parameters_->shared_tree_worker_enable_trail_sharing()) return;
  if (sat_solver_->ModelIsUnsat()) return;
  if (time_limit_->LimitReached()) return;
  DCHECK(sat_solver_->PropagationIsDone());
  const int binary_propagator_id = binary_propagator_->PropagatorId();
  DCHECK_GE(trail_->Index(), reversible_trail_index_);
  for (int i = reversible_trail_index_; i < trail_->Index(); ++i) {
    const Literal literal = (*trail_)[i];
    if (trail_->AssignmentLevel(literal) == 0) {
      // Literals true at level zero won't be reprocessed after restart (which
      // invalidates node pointers). Store null for root assignments to avoid
      // dangling pointers.
      fixed_by_[literal.Index()] = nullptr;
      continue;
    }
    const int assignment_type = trail_->AssignmentType(literal.Variable());
    if (assignment_type == AssignmentType::kSearchDecision) {
      if (fixed_by_[literal.Index()] != nullptr) continue;
      reversible_int_repository_->SaveState(&reversible_trail_index_);
      reversible_trail_index_ = i;
      return;
    }

    SharedTreeEncoder::Node* result = assigned_nodes_.front();
    for (Literal reason_literal : trail_->Reason(literal.Variable())) {
      SharedTreeEncoder::Node* node = FixedByOrNull(reason_literal.Negated());
      DCHECK_NE(node, nullptr);
      result = std::max(node, result, &NodeIdLess);
    }
    DCHECK(!result->shared().is_closed());
    fixed_by_[literal] = result;
    // Avoid exporting things propagated by binary clauses, since these are
    // already shared normally.
    if (assignment_type == binary_propagator_id) continue;
    std::optional<ProtoLiteral> encoded = tree_.Encode(literal);
    if (!encoded.has_value()) continue;
    if (result->AddImplication(*encoded) && lrat_proof_handler_ != nullptr) {
      result->ExportInferredReasonClause(literal, GetProof(result, literal));
    }
  }
  if (!assigned_nodes_.empty()) {
    // We've reached the end of the trail, so the current lower bound is valid
    // for the last assigned node.
    assigned_nodes_.back()->SetObjectiveLowerBound(
        CurrentObjectiveLowerBound());
  }
  reversible_int_repository_->SaveState(&reversible_trail_index_);
  reversible_trail_index_ = trail_->Index();
}

absl::Span<const ClausePtr> SharedTreeWorker::GetProof(
    SharedTreeEncoder::Node* implied_by, Literal literal) {
  tmp_proof_clauses_.clear();
  if (lrat_proof_handler_ == nullptr) return tmp_proof_clauses_;
  DCHECK(!sat_solver_->ModelIsUnsat());
  DCHECK(!implied_by->shared().is_closed());
  clause_manager_->AppendClausesFixing(
      {literal}, &tmp_proof_clauses_,
      /*decision=*/kNoLiteralIndex, [&](int level, int index) -> ClausePtr {
        if (level == 0) return kNullClausePtr;
        const Literal lit = (*trail_)[index];
        SharedTreeEncoder::Node* node = FixedByOrNull(lit);
        if (node == nullptr) return kNullClausePtr;
        // Only use a cached reason if `node` is no deeper than `implied_by`,
        // otherwise the reason will not be unit.
        if (NodeIdLess(implied_by, node)) return kNullClausePtr;
        return node->ReasonClauseOrNull(lit);
      });
  return tmp_proof_clauses_;
}

bool SharedTreeWorker::ResetAndEnqueueAssumptions(NodeId leaf_id) {
  leaf_id_ = leaf_id;
  assigned_nodes_.clear();
  fixed_by_.assign(2 * sat_solver_->NumVariables(), nullptr);
  if (!sat_solver_->ResetToLevelZero()) return false;
  if (time_limit_->LimitReached()) return true;
  if (!helper_->BeforeTakingDecision()) return false;
  reversible_trail_index_ = trail_->Index();
  const VariablesAssignment& assignment = sat_solver_->Assignment();
  for (SharedTreeEncoder::Node* node : tree_.GetLevelStartNodes(leaf_id)) {
    if (node->shared().is_closed()) return sat_solver_->ResetToLevelZero();
    tree_.NormalizeReasonClauses(node);
    if (node->shared().is_root()) {
      assigned_nodes_.push_back(node);
    } else {
      const Literal decision = node->decoded_decision();
      if (assignment.LiteralIsTrue(decision)) {
        // The decision is implied, so the sibling can be closed.
        tree_.CloseNode(node->shared().sibling_id(),
                        GetProof(assigned_nodes_.back(), decision));
        node = node->GetLevelStart();
      } else {
        assigned_nodes_.push_back(node);
        fixed_by_[decision] = node;
        if (!sat_solver_->EnqueueAssumption(decision)) {
          return CloseNodeAfterConflict();
        }
      }
    }
    for (const auto [var, lb] : node->shared().var_lower_bounds) {
      const Literal implied = tree_.Decode(ProtoLiteral(var, lb));
      if (assignment.LiteralIsTrue(implied)) continue;
      *trail_->GetEmptyVectorToStoreReason() = node->NegatedDecisions();
      if (!trail_->EnqueueWithStoredReason(implied,
                                           node->ReasonClauseOrNull(implied))) {
        return ProcessAssumptionConflict();
      }
    }

    // Ensure equivalent literals are always fixed at the correct node.
    if (!binary_propagator_->Propagate(trail_)) {
      return ProcessAssumptionConflict();
    }

    // Ensure propagation is finished at the root, or if explicitly enabled.
    if (trail_->CurrentDecisionLevel() == 0 ||
        parameters_->shared_tree_worker_propagate_intermediate_nodes()) {
      if (!sat_solver_->FinishPropagation()) return CloseNodeAfterConflict();
      ExportNewImplications();
    }
  }

  if (!assigned_nodes_.empty()) {
    const IntegerValue shared_objective_lb =
        assigned_nodes_.back()->shared().objective_lb;
    if (shared_objective_lb > CurrentObjectiveLowerBound()) {
      const Literal objective_lit =
          encoder_->GetOrCreateAssociatedLiteral(IntegerLiteral::GreaterOrEqual(
              objective_->objective_var, shared_objective_lb));
      *trail_->GetEmptyVectorToStoreReason() =
          assigned_nodes_.back()->NegatedDecisions();
      if (!trail_->EnqueueWithStoredReason(
              objective_lit,
              assigned_nodes_.back()->ReasonClauseOrNull(objective_lit))) {
        return ProcessAssumptionConflict();
      }
    }
  }

  if (!sat_solver_->FinishPropagation()) {
    return CloseNodeAfterConflict();
  }
  ExportNewImplications();
  return true;
}

bool SharedTreeWorker::ProcessAssumptionConflict() {
  sat_solver_->ProcessCurrentConflict();
  return CloseNodeAfterConflict();
}

bool SharedTreeWorker::CloseNodeAfterConflict() {
  if (sat_solver_->ModelIsUnsat()) return false;
  if (time_limit_->LimitReached()) return true;
  std::vector<Literal> core = sat_solver_->GetLastIncompatibleDecisions();
  DCHECK(!core.empty());
  SharedTreeEncoder::Node* node_to_close = FixedByOrNull(core.back());
  DCHECK_NE(node_to_close, nullptr);
  SharedTreeEncoder::Node* implied_by =
      core.size() == 1 ? assigned_nodes_.front()
                       : FixedByOrNull(core[core.size() - 2]);
  DCHECK_NE(implied_by, nullptr);
  DCHECK(!implied_by->shared().is_closed());
  if (implied_by->AddImplication(node_to_close->shared().decision.Negated()) &&
      lrat_proof_handler_ != nullptr) {
    implied_by->ExportInferredReasonClause(
        core.back().Negated(),
        {sat_solver_->GetLastIncompatibleDecisionsAsClausePtr()});
  }
  tree_.CloseNode(node_to_close->shared().id,
                  {sat_solver_->GetLastIncompatibleDecisionsAsClausePtr()});
  return sat_solver_->ResetToLevelZero();
}
}  // namespace operations_research::sat
