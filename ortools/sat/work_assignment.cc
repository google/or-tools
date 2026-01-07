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
#include <array>
#include <cmath>
#include <deque>
#include <functional>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "absl/functional/function_ref.h"
#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/strings/str_cat.h"
#include "absl/synchronization/mutex.h"
#include "absl/types/span.h"
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
#include "ortools/util/strong_integers.h"
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
}  // namespace

Literal ProtoLiteral::Decode(CpModelMapping* mapping,
                             IntegerEncoder* encoder) const {
  DCHECK_LT(proto_var_, mapping->NumProtoVariables());
  if (mapping->IsBoolean(proto_var_)) {
    return mapping->Literal(proto_var_);
  }
  return encoder->GetOrCreateAssociatedLiteral(DecodeInteger(mapping));
}

IntegerLiteral ProtoLiteral::DecodeInteger(CpModelMapping* mapping) const {
  const int positive_var = PositiveRef(proto_var_);
  if (!mapping->IsInteger(positive_var)) {
    return IntegerLiteral();
  }
  if (proto_var_ < 0) {
    return IntegerLiteral::LowerOrEqual(mapping->Integer(positive_var), -lb_);
  }
  return IntegerLiteral::GreaterOrEqual(mapping->Integer(positive_var), lb_);
}

std::optional<ProtoLiteral> ProtoLiteral::EncodeInteger(
    IntegerLiteral literal, CpModelMapping* mapping) {
  IntegerVariable positive_var = PositiveVariable(literal.var);
  const int model_var =
      mapping->GetProtoVariableFromIntegerVariable(positive_var);
  if (model_var == -1) {
    return std::nullopt;
  }
  ProtoLiteral result{
      literal.var == positive_var ? model_var : NegatedRef(model_var),
      literal.bound};
  DCHECK_EQ(result.DecodeInteger(mapping), literal);
  DCHECK_EQ(result.Negated().DecodeInteger(mapping), literal.Negated());
  return result;
}
std::optional<ProtoLiteral> ProtoLiteral::Encode(Literal literal,
                                                 CpModelMapping* mapping,
                                                 IntegerEncoder* encoder) {
  const std::optional<ProtoLiteral> result = EncodeLiteral(literal, mapping);
  if (result.has_value()) return result;

  for (auto int_lit : encoder->GetIntegerLiterals(literal)) {
    auto result = EncodeInteger(int_lit, mapping);
    if (result.has_value()) {
      DCHECK_EQ(result->DecodeInteger(mapping), int_lit);
      DCHECK_EQ(result->Negated().DecodeInteger(mapping), int_lit.Negated());
      return result;
    }
  }
  return std::nullopt;
}

std::optional<ProtoLiteral> ProtoLiteral::EncodeLiteral(
    Literal literal, CpModelMapping* mapping) {
  if (literal.Index() == kNoLiteralIndex) {
    return std::nullopt;
  }
  int model_var =
      mapping->GetProtoVariableFromBooleanVariable(literal.Variable());
  if (model_var == -1) {
    return std::nullopt;
  }
  DCHECK(mapping->IsBoolean(model_var));
  ProtoLiteral result{literal.IsPositive() ? model_var : NegatedRef(model_var),
                      literal.IsPositive() ? 1 : 0};
  return result;
}

namespace {
Literal DecodeWithIdentityMapping(const ProtoLiteral& literal) {
  const int ref = literal.proto_var();
  return Literal(BooleanVariable(PositiveRef(ref)), RefIsPositive(ref));
}
}  // namespace

ProtoTrail::ProtoTrail() { target_phase_.reserve(kMaxPhaseSize); }

void ProtoTrail::PushLevel(const ProtoLiteral& decision,
                           IntegerValue objective_lb, int node_id) {
  CHECK_GT(node_id, 0);
  decision_indexes_.push_back(literals_.size());
  assigned_at_level_[decision] = decision_indexes_.size();
  literals_.push_back(decision);
  node_ids_.push_back(node_id);
  implications_.push_back({});
  if (!level_to_objective_lbs_.empty()) {
    objective_lb = std::max(level_to_objective_lbs_.back(), objective_lb);
  }
  level_to_objective_lbs_.push_back(objective_lb);
}

void ProtoTrail::SetLevelImplied(int level) {
  DCHECK_GE(level, 1);
  DCHECK_LE(level, decision_indexes_.size());
  DCHECK_LE(level, implications_.size());
  SetObjectiveLb(level - 1, ObjectiveLb(level));
  const ProtoLiteral decision = Decision(level);
  assigned_at_level_[decision] = level - 1;
  // We don't store implications for level 0, so only move implications up to
  // the parent if we are removing level 2 or greater.
  if (level >= 2) {
    MutableImplications(level - 1).push_back(decision);
  }
  for (const ProtoLiteral& implication : Implications(level)) {
    assigned_at_level_[implication] = level - 1;
    if (level >= 2) {
      MutableImplications(level - 1).push_back(implication);
    }
  }
  // implications_[level-1] stores the implications for level, which are now
  // stored in the parent's implications, so we can delete them.
  implications_.erase(implications_.begin() + level - 1);
  decision_indexes_.erase(decision_indexes_.begin() + level - 1);
  level_to_objective_lbs_.erase(level_to_objective_lbs_.begin() + level - 1);
}

void ProtoTrail::NormalizeImplications() {
  assigned_at_level_.clear();
  for (int level = 1; level <= MaxLevel(); ++level) {
    assigned_at_level_[Decision(level)] = level;
    int new_size = 0;
    std::vector<ProtoLiteral>& implications = MutableImplications(level);
    for (int i = 0; i < implications.size(); ++i) {
      const ProtoLiteral& implication = implications[i];
      if (!assigned_at_level_.contains(implication)) {
        implications[new_size++] = implication;
        assigned_at_level_[implication] = level;
      }
    }
    implications.resize(new_size);
  }
}

void ProtoTrail::Clear() {
  decision_indexes_.clear();
  literals_.clear();
  level_to_objective_lbs_.clear();
  node_ids_.clear();
  target_phase_.clear();
  assigned_at_level_.clear();
  implications_.clear();
}

void ProtoTrail::SetObjectiveLb(int level, IntegerValue objective_lb) {
  if (level == 0) return;
  level_to_objective_lbs_[level - 1] =
      std::max(objective_lb, level_to_objective_lbs_[level - 1]);
}

int ProtoTrail::DecisionNodeId(int level) const {
  DCHECK_LE(level, decision_indexes_.size());
  return node_ids_[decision_indexes_[level - 1]];
}

absl::Span<const int> ProtoTrail::NodeIds(int level) const {
  DCHECK_LE(level, decision_indexes_.size());
  int start = level == 0 ? 0 : decision_indexes_[level - 1];
  int end = level == decision_indexes_.size() ? node_ids_.size()
                                              : decision_indexes_[level];
  return absl::MakeSpan(node_ids_.data() + start, end - start);
}

absl::Span<const ProtoLiteral> ProtoTrail::Implications(int level) const {
  if (level > implications_.size() || level <= 0) {
    return absl::MakeSpan(literals_.data(), 0);
  }
  return absl::MakeSpan(implications_[level - 1]);
}

SharedTreeManager::SharedTreeManager(Model* model)
    : params_(*model->GetOrCreate<SatParameters>()),
      num_workers_(std::max(0, params_.shared_tree_num_workers())),
      max_path_depth_(MaxPossibleLeafDepth(params_)),
      shared_response_manager_(model->GetOrCreate<SharedResponseManager>()),
      lrat_proof_handler_(LratProofHandler::MaybeCreate(
          params_, &clause_id_generator_,
          model->GetOrCreate<SharedLratProofStatus>(),
          model->GetOrCreate<SharedStatistics>())),
      num_splits_wanted_(
          num_workers_ * params_.shared_tree_open_leaves_per_worker() - 1),
      max_nodes_(
          params_.shared_tree_max_nodes_per_worker() >=
                  std::numeric_limits<int>::max() / std::max(num_workers_, 1)
              ? std::numeric_limits<int>::max()
              : num_workers_ * params_.shared_tree_max_nodes_per_worker()) {
  // Create the root node with a fake decision.
  nodes_.push_back(
      {.decision = ProtoLiteral(),
       .objective_lb = shared_response_manager_->GetInnerObjectiveLowerBound(),
       .trail_info = std::make_unique<NodeTrailInfo>()});
  unassigned_leaves_.push_back(&nodes_.back());
}

int SharedTreeManager::NumNodes() const {
  absl::MutexLock mutex_lock(mu_);
  return nodes_.size();
}

bool SharedTreeManager::SyncTree(ProtoTrail& path) {
  absl::MutexLock mutex_lock(mu_);
  std::vector<std::pair<Node*, int>> nodes = GetAssignedNodes(path);
  if (!IsValid(path)) {
    path.Clear();
    return false;
  }
  DCHECK(CheckLratInvariants());
  // We don't rely on these being empty, but we expect them to be.
  DCHECK(to_close_.empty());
  DCHECK(to_update_.empty());
  int prev_level = -1;
  for (const auto& [node, level] : nodes) {
    if (level == prev_level) {
      // `node` is implied by the previous decisions in `path`, hence its
      // sibling can be closed (using this implication as proof; the implication
      // proved by the worker providing `path` must be imported and a new one,
      // adapted for the manager, must be inferred from it).
      Node* sibling = GetSibling(node);
      ClauseId closing_clause_id = kNoClauseId;
      if (lrat_proof_handler_ != nullptr) {
        // For the worker, `node` is implied by all the previous decisions in
        // `path`, but for the manager we need an implication clause using the
        // non-implied ancestors of `node` in the tree (they can be different
        // because the manager and the worker have different views of the tree).
        const std::vector<Literal> inferred_clause = ClosingClause(sibling);
        std::vector<Literal> imported_clause;
        std::vector<ClauseId> lrat_proof;
        for (int l = 1; l <= level + 1; ++l) {
          Node* n = l <= level ? GetNode(path.DecisionNodeId(l)) : node;
          const Literal decision = DecodeWithIdentityMapping(n->decision);
          imported_clause.push_back(l <= level ? decision.Negated() : decision);
          if (n->implied_and_processed) {
            lrat_proof.push_back(GetSibling(n)->closing_clause_id);
          }
        }
        closing_clause_id = AddImportedAndInferredClauses(
            imported_clause, inferred_clause, lrat_proof);
      }
      to_close_.emplace_back(sibling, closing_clause_id);
    } else if (level > 0 && node->objective_lb < path.ObjectiveLb(level)) {
      node->objective_lb = path.ObjectiveLb(level);
      to_update_.push_back(node->parent);
    }
    if (level > 0 && !node->closed) {
      NodeTrailInfo* trail_info = GetTrailInfo(node);
      for (const ProtoLiteral& implication : path.Implications(level)) {
        // Trivial implication, can be ignored.
        if (IsDecisionOfNodeOrAncestor(implication, node)) continue;
        ClauseId implication_clause_id = kNoClauseId;
        if (lrat_proof_handler_ != nullptr) {
          // For the worker, 'implication' is implied by all the previous
          // decisions in `path`, but for the manager we need an implication
          // clause using the non-implied ancestors of `node` in the tree (they
          // can be different because the manager and the worker have different
          // views of the tree).
          const std::vector<Literal> inferred_clause =
              ImplicationClause(node, implication);
          std::vector<Literal> imported_clause;
          std::vector<ClauseId> lrat_proof;
          for (int l = 1; l <= level; ++l) {
            Node* n = GetNode(path.DecisionNodeId(l));
            const Literal decision = DecodeWithIdentityMapping(n->decision);
            imported_clause.push_back(decision.Negated());
            if (n->implied_and_processed) {
              lrat_proof.push_back(GetSibling(n)->closing_clause_id);
            }
          }
          imported_clause.push_back(DecodeWithIdentityMapping(implication));
          implication_clause_id = AddImportedAndInferredClauses(
              imported_clause, inferred_clause, lrat_proof);
        }
        auto it = trail_info->implications
                      .emplace(implication.proto_var(),
                               std::make_pair(implication.lb(),
                                              implication_clause_id))
                      .first;
        if (it->second.first < implication.lb()) {
          it->second.first = implication.lb();
        }
      }
    }
    prev_level = level;
  }
  ProcessNodeChanges();
  if (nodes.back().first->closed) {
    path.Clear();
    return false;
  }
  // Restart after processing updates - we might learn a new objective bound.
  // Do initial restarts once each worker has had the chance to be assigned a
  // leaf.
  if (num_leaves_assigned_since_restart_ >= num_workers_ &&
      num_restarts_ < kNumInitialRestarts) {
    RestartLockHeld();
    path.Clear();
    return false;
  }
  // Sync lower bounds and implications from the shared tree to `path`.
  AssignLeaf(path, nodes.back().first);
  DCHECK(CheckLratInvariants());
  return true;
}

int SharedTreeManager::TrySplitTree(absl::Span<const ProtoLiteral> decisions,
                                    ProtoTrail& path) {
  decisions = decisions.subspan(0, max_path_depth_ - path.MaxLevel());
  if (decisions.empty()) return 0;
  absl::MutexLock l(mu_);
  for (int i = 0; i < decisions.size(); ++i) {
    if (!TrySplitTreeLockHeld(decisions[i], path)) return i;
  }
  return decisions.size();
}

bool SharedTreeManager::TrySplitTreeLockHeld(ProtoLiteral decision,
                                             ProtoTrail& path) {
  if (!IsValid(path)) return false;
  std::vector<std::pair<Node*, int>> nodes = GetAssignedNodes(path);
  if (nodes.back().first->closed) {
    VLOG(2) << "Cannot split closed node";
    return false;
  }
  if (nodes.back().first->children[0] != nullptr) {
    LOG_IF(WARNING, nodes.size() > 1)
        << "Cannot resplit previously split node @ " << nodes.back().second
        << "/" << nodes.size();
    return false;
  }
  if (nodes_.size() + 2 > max_nodes_) {
    VLOG(2) << "Too many nodes to accept split";
    return false;
  }
  if (num_splits_wanted_ <= 0) {
    VLOG(2) << "Enough splits for now";
    return false;
  }
  for (const auto& [node, level] : nodes) {
    if (decision == node->decision || decision == node->decision.Negated()) {
      VLOG(2) << "Cannot split on decision which is already in the tree";
      return false;
    }
  }
  if (params_.shared_tree_split_strategy() ==
          SatParameters::SPLIT_STRATEGY_DISCREPANCY ||
      params_.shared_tree_split_strategy() ==
          SatParameters::SPLIT_STRATEGY_AUTO) {
    int discrepancy = 0;
    for (const auto& [node, level] : nodes) {
      if (node->parent == nullptr || node->implied) continue;
      IntegerValue sibling_bound = GetSibling(node)->objective_lb;
      discrepancy += (node->objective_lb == sibling_bound
                          ? node != node->parent->children[0]
                          : node->objective_lb > sibling_bound);
    }
    // TODO(user): Need to write up the shape this creates.
    // This rule will allow twice as many leaves in the preferred subtree.
    if (discrepancy + path.MaxLevel() >= max_path_depth_) {
      VLOG(2) << "Too high discrepancy to accept split";
      return false;
    }
  } else if (params_.shared_tree_split_strategy() ==
             SatParameters::SPLIT_STRATEGY_OBJECTIVE_LB) {
    if (nodes.back().first->objective_lb > nodes.front().first->objective_lb) {
      VLOG(2) << "Can only split nodes with minimum objective lb, "
              << nodes.back().first->objective_lb << " > "
              << nodes.front().first->objective_lb;
      return false;
    }
  }
  VLOG_EVERY_N(2, 10) << unassigned_leaves_.size() << " unassigned leaves, "
                      << nodes_.size() << " subtrees, " << num_splits_wanted_
                      << " splits wanted";
  Split(nodes, decision);
  auto [new_leaf, level] = nodes.back();
  path.PushLevel(new_leaf->decision, new_leaf->objective_lb, new_leaf->id);
  return true;
}

void SharedTreeManager::ReplaceTree(ProtoTrail& path) {
  absl::MutexLock mutex_lock(mu_);
  std::vector<std::pair<Node*, int>> nodes = GetAssignedNodes(path);
  if (nodes.back().first->children[0] == nullptr &&
      !nodes.back().first->closed && nodes.size() > 1) {
    Node* leaf = nodes.back().first;
    VLOG(2) << "Returning leaf to be replaced";
    GetTrailInfo(leaf)->phase = path.TakeTargetPhase();
    unassigned_leaves_.push_back(leaf);
  }
  path.Clear();
  while (!unassigned_leaves_.empty()) {
    Node* leaf = unassigned_leaves_.front();
    unassigned_leaves_.pop_front();
    if (!leaf->closed && leaf->children[0] == nullptr) {
      num_leaves_assigned_since_restart_ += 1;
      AssignLeaf(path, leaf);
      path.SetTargetPhase(std::move(GetTrailInfo(leaf)->phase));
      return;
    }
  }
  VLOG(2) << "Assigning root because no unassigned leaves are available";
  // TODO(user): Investigate assigning a random leaf so workers can still
  // improve shared tree bounds.
}

SharedTreeManager::NodeTrailInfo* SharedTreeManager::GetTrailInfo(Node* node) {
  CHECK(node != nullptr && !node->closed);
  while (node->trail_info == nullptr) {
    node = node->parent;
  }
  CHECK_NE(node, nullptr);
  return node->trail_info.get();
}

void SharedTreeManager::ClearTrailInfo(Node* node, bool implications_only) {
  if (node->trail_info == nullptr) return;
  if (lrat_proof_handler_ != nullptr) {
    for (const auto& [var, lb_and_clause] : node->trail_info->implications) {
      lrat_proof_handler_->DeleteClause(lb_and_clause.second, {});
    }
  }
  if (implications_only) {
    node->trail_info->implications.clear();
  } else {
    node->trail_info.reset();
  }
}

SharedTreeManager::Node* SharedTreeManager::GetSibling(const Node* node) const {
  if (node == nullptr || node->parent == nullptr) return nullptr;
  if (node->parent->children[0] != node) {
    return node->parent->children[0];
  }
  return node->parent->children[1];
}

void SharedTreeManager::Split(std::vector<std::pair<Node*, int>>& nodes,
                              ProtoLiteral lit) {
  const auto [parent, level] = nodes.back();
  DCHECK(parent->children[0] == nullptr);
  DCHECK(parent->children[1] == nullptr);
  parent->children[0] = MakeSubtree(parent, lit);
  parent->children[1] = MakeSubtree(parent, lit.Negated());
  NodeTrailInfo* trail_info = GetTrailInfo(parent);
  if (trail_info != nullptr) {
    parent->children[0]->trail_info =
        std::make_unique<NodeTrailInfo>(NodeTrailInfo{});
    parent->children[1]->trail_info = std::make_unique<NodeTrailInfo>(
        NodeTrailInfo{.phase = std::move(trail_info->phase)});
  }
  nodes.push_back(std::make_pair(parent->children[0], level + 1));
  unassigned_leaves_.push_back(parent->children[1]);
  --num_splits_wanted_;
}

SharedTreeManager::Node* SharedTreeManager::MakeSubtree(Node* parent,
                                                        ProtoLiteral decision) {
  nodes_.push_back(
      Node{.decision = decision,
           .objective_lb = parent->objective_lb,
           .parent = parent,
           .id = static_cast<int>(nodes_.size() + node_id_offset_)});
  return &nodes_.back();
}

void SharedTreeManager::ProcessNodeChanges() {
  DCHECK(CheckLratInvariants());
  int num_newly_closed = 0;
  std::vector<Node*> newly_implied;
  while (!to_close_.empty()) {
    auto [node, closing_clause_id] = to_close_.back();
    CHECK_NE(node, nullptr);
    to_close_.pop_back();
    // Iterate over open parents while each sibling is closed.
    while (node != nullptr && !node->closed) {
      ++num_newly_closed;
      ++num_closed_nodes_;
      node->closed = true;
      node->closing_clause_id = closing_clause_id;
      // Keep the root trail_info so GetTrailInfo never returns nullptr.
      if (node->parent != nullptr) {
        ClearTrailInfo(node);
      }
      node->objective_lb = kMaxIntegerValue;
      // If we are closing a leaf, try to maintain the same number of leaves;
      num_splits_wanted_ += (node->children[0] == nullptr);
      for (Node* child : node->children) {
        if (child == nullptr || child->closed) continue;
        ClauseId child_closing_clause_id = kNoClauseId;
        if (lrat_proof_handler_ != nullptr) {
          // The node's closing clause is sufficient to prove that `child` can
          // be closed. We use a new clause only to avoid double deletes in
          // RestartLockHeld().
          child_closing_clause_id = clause_id_generator_.GetNextId();
          lrat_proof_handler_->AddInferredClause(
              child_closing_clause_id, ClosingClause(child),
              {closing_clause_id}, /*exported=*/true);
        }
        to_close_.emplace_back(child, child_closing_clause_id);
      }
      Node* sibling = GetSibling(node);
      if (sibling != nullptr) {
        sibling->implied = true;
        if (lrat_proof_handler_ != nullptr) {
          newly_implied.push_back(sibling);
        }
        if (!sibling->closed) {
          break;
        }
      }
      Node* parent = node->parent;
      if (lrat_proof_handler_ != nullptr && parent != nullptr &&
          !parent->closed) {
        closing_clause_id = clause_id_generator_.GetNextId();
        // Combine the clauses proving that the node and its sibling could be
        // closed to prove that the parent can be closed.
        lrat_proof_handler_->AddInferredClause(
            closing_clause_id, ClosingClause(parent),
            {node->closing_clause_id, sibling->closing_clause_id},
            /*exported=*/true);
      }
      node = parent;
    }
    DCHECK(node == nullptr || node->closed);
    if (node == nullptr) {
      shared_response_manager_->NotifyThatImprovingProblemIsInfeasible(
          ShortStatus());
    } else if (node->parent != nullptr) {
      to_update_.push_back(node->parent);
    }
  }
  if (num_newly_closed > 0) {
    shared_response_manager_->LogMessageWithThrottling(
        "Tree", absl::StrCat("closed:", num_closed_nodes_, "/", nodes_.size(),
                             " unassigned:", unassigned_leaves_.size(),
                             " restarts:", num_restarts_));
  }
  DCHECK(CheckLratInvariants());
  // TODO(user): We could do resolution here by moving implications that
  // are true in each child to the parent.
  bool root_updated = false;
  while (!to_update_.empty()) {
    Node* node = to_update_.back();
    to_update_.pop_back();
    // Iterate over parents while the lower bound can be improved.
    while (node != nullptr && !node->closed) {
      DCHECK(node->children[0] != nullptr);
      DCHECK(node->children[1] != nullptr);
      for (Node* child : node->children) {
        if (child->implied) {
          if (child->trail_info != nullptr) {
            DCHECK(!child->implied_and_processed);
            ProcessImpliedNode(child);
            ClearTrailInfo(child);
          }
          child->implied_and_processed = true;
        }
      }
      IntegerValue child_bound = std::min(node->children[0]->objective_lb,
                                          node->children[1]->objective_lb);
      if (child_bound <= node->objective_lb) break;
      node->objective_lb = child_bound;
      node = node->parent;
    }
    if (node == nullptr) root_updated = true;
  }
  if (root_updated) {
    shared_response_manager_->UpdateInnerObjectiveBounds(
        ShortStatus(), nodes_[0].objective_lb, kMaxIntegerValue);
  }
  for (Node* node : newly_implied) {
    if (!node->implied_and_processed) {
      DCHECK_EQ(node->trail_info, nullptr);
      DCHECK_NE(lrat_proof_handler_, nullptr);
      ProcessImpliedNode(node);
      node->implied_and_processed = true;
    }
  }
  // These are shared via SharedBoundsManager, don't duplicate here.
  ClearTrailInfo(&nodes_[0], /*implications_only=*/true);
  DCHECK(CheckLratInvariants());
}

// Moves the trail_info implications of `node` to its first non-implied
// ancestor, and removes the newly implied literal from the closing and
// implication clauses of `node` and its descendants.
void SharedTreeManager::ProcessImpliedNode(Node* node) {
  CHECK(node->parent != nullptr);
  Node* first_non_implied_ancestor = node->parent;
  while (first_non_implied_ancestor->trail_info == nullptr) {
    first_non_implied_ancestor = first_non_implied_ancestor->parent;
    DCHECK_NE(first_non_implied_ancestor, nullptr);
  }
  // Fast path for the common case where there is no need to add LRAT clauses.
  // The rest of the code is only executed when LRAT is enabled, and assumes a
  // pure SAT problem.
  if (lrat_proof_handler_ == nullptr) {
    first_non_implied_ancestor->trail_info->implications.merge(
        node->trail_info->implications);
    return;
  }
  // Gather the clauses needed to prove the new implications and closing
  // clauses.
  std::vector<ClauseId> clauses;
  Node* n = node;
  while (n->parent != nullptr) {
    // Newly implied nodes must be removed from the closing and implication
    // clauses, which requires a proof (already implied nodes are no longer in
    // these clauses, so we don't need a proof for them).
    if (n->implied && !n->implied_and_processed) {
      clauses.push_back(GetSibling(n)->closing_clause_id);
    }
    n = n->parent;
  }
  std::reverse(clauses.begin(), clauses.end());
  // Move the implications of `node` to the first non-implied ancestor.
  if (node->trail_info != nullptr) {
    for (const auto& [var, lb_and_clause] : node->trail_info->implications) {
      // This is OK because we assume a pure SAT problem.
      if (first_non_implied_ancestor->trail_info->implications.contains(var)) {
        continue;
      }
      const auto [lb, clause_id] = lb_and_clause;
      ClauseId new_clause_id = clause_id_generator_.GetNextId();
      clauses.push_back(clause_id);
      lrat_proof_handler_->AddInferredClause(
          new_clause_id,
          ImplicationClause(first_non_implied_ancestor, ProtoLiteral(var, lb),
                            /*skip_unprocessed_implied_nodes=*/true),
          clauses, /*exported=*/true);
      clauses.pop_back();
      first_non_implied_ancestor->trail_info->implications.insert(
          {var, std::make_pair(lb, new_clause_id)});
    }
  }
  UpdateLratClausesInSubtree(node, node, clauses);
}

// Updates the closing clauses and the trail implication clauses of all the
// nodes in the subtree rooted at `node`, to maintain the LRAT invariants.
// Recursive method where `n` is a node of the subtree, and `clauses` are the
// clauses needed to infer its updated closing and implication clauses.
// TODO(user): change to a non-recursive implementation?
void SharedTreeManager::UpdateLratClausesInSubtree(
    Node* node, Node* n, std::vector<ClauseId>& clauses) {
  const bool implied_and_not_processed =
      n->implied && !n->implied_and_processed;
  if (implied_and_not_processed) {
    // Newly implied nodes must be removed from the closing and implication
    // clauses of `n`, which requires a proof (already implied nodes are no
    // longer in these clauses, so we don't need a proof for them).
    clauses.push_back(GetSibling(n)->closing_clause_id);
  }
  if (n->closed) {
    DCHECK_NE(n->closing_clause_id, kNoClauseId);
    ClauseId new_clause_id = clause_id_generator_.GetNextId();
    clauses.push_back(n->closing_clause_id);
    lrat_proof_handler_->AddInferredClause(
        new_clause_id,
        ClosingClause(n, /*skip_unprocessed_implied_nodes=*/true), clauses,
        /*exported=*/true);
    clauses.pop_back();
    lrat_proof_handler_->DeleteClause(n->closing_clause_id, {});
    n->closing_clause_id = new_clause_id;
  }
  if (n != node && n->trail_info != nullptr) {
    for (auto& [var, lb_and_clause] : n->trail_info->implications) {
      auto& [lb, clause_id] = lb_and_clause;
      ClauseId new_clause_id = clause_id_generator_.GetNextId();
      clauses.push_back(clause_id);
      lrat_proof_handler_->AddInferredClause(
          new_clause_id,
          ImplicationClause(n, ProtoLiteral(var, lb),
                            /*skip_unprocessed_implied_nodes=*/true),
          clauses, /*exported=*/true);
      lrat_proof_handler_->DeleteClause(clause_id, {});
      clause_id = new_clause_id;
      clauses.pop_back();
    }
  }
  // We can stop at implied but not yet processed nodes (they will be processed
  // with further calls to ProcessImpliedNode()).
  if (n == node || !(n->implied && n->trail_info != nullptr)) {
    for (Node* child : n->children) {
      if (child != nullptr && child->parent != nullptr) {
        UpdateLratClausesInSubtree(node, child, clauses);
      }
    }
  }
  if (implied_and_not_processed) {
    clauses.pop_back();
  }
}

SharedTreeManager::Node* SharedTreeManager::GetNode(int id) {
  const int index = id - node_id_offset_;
  CHECK_GE(index, 0);
  CHECK_LT(index, nodes_.size());
  return &nodes_[index];
}

std::vector<std::pair<SharedTreeManager::Node*, int>>
SharedTreeManager::GetAssignedNodes(const ProtoTrail& path) {
  std::vector<std::pair<Node*, int>> nodes({std::make_pair(&nodes_[0], 0)});
  if (!IsValid(path)) {
    // Restart has happened, nodes in this path are no longer valid, but the
    // root is equivalent.
    return nodes;
  }
  for (int i = 0; i <= path.MaxLevel(); ++i) {
    for (int id : path.NodeIds(i)) {
      const int index = id - node_id_offset_;
      CHECK_GE(index, 0) << " in path.NodeIds(" << i
                         << "), max_level=" << path.MaxLevel();
      CHECK_LT(index, nodes_.size());
      DCHECK_EQ(nodes.back().first, nodes_[index].parent);
      nodes.push_back(std::make_pair(&nodes_[index], i));
    }
  }
  return nodes;
}

void SharedTreeManager::CloseTree(ProtoTrail& path, int level) {
  absl::MutexLock mutex_lock(mu_);
  DCHECK(CheckLratInvariants());
  const int node_id_to_close = path.NodeIds(level).front();
  if (node_id_to_close < node_id_offset_) {
    path.Clear();
    return;
  }
  Node* node = &nodes_[node_id_to_close - node_id_offset_];
  VLOG(2) << "Closing subtree at level " << level;
  DCHECK(to_close_.empty());

  ClauseId closing_clause_id = kNoClauseId;
  if (lrat_proof_handler_ != nullptr) {
    // For the worker providing `path`, `node` is implied by all the previous
    // decisions in `path`, but for the manager we need a closing clause using
    // `node` and its ancestors in the tree (with implied ones filtered out --
    // they can be different because the manager and the worker have different
    // views of the tree).
    const std::vector<Literal> inferred_clause = ClosingClause(node);
    std::vector<Literal> imported_clause;
    std::vector<ClauseId> lrat_proof;
    for (int l = 1; l <= level; ++l) {
      Node* n = GetNode(path.DecisionNodeId(l));
      const Literal decision = DecodeWithIdentityMapping(n->decision);
      imported_clause.push_back(decision.Negated());
      if (n->implied_and_processed) {
        lrat_proof.push_back(GetSibling(n)->closing_clause_id);
      }
    }
    closing_clause_id = AddImportedAndInferredClauses(
        imported_clause, inferred_clause, lrat_proof);
  }
  path.Clear();
  to_close_.emplace_back(node, closing_clause_id);
  ProcessNodeChanges();
  DCHECK(CheckLratInvariants());
}

bool SharedTreeManager::IsDecisionOfNodeOrAncestor(ProtoLiteral literal,
                                                   const Node* node) const {
  CHECK_NE(node, nullptr);
  while (node->parent != nullptr) {
    if (literal == node->decision) return true;
    node = node->parent;
  }
  return false;
}

std::vector<Literal> SharedTreeManager::ImplicationClause(
    const Node* node, ProtoLiteral implied,
    bool skip_unprocessed_implied_nodes) const {
  // This is only used for LRAT, which only works for pure SAT, without
  // presolve. In this case all workers should have the same identity mapping
  // from the proto variables.
  CHECK_NE(node, nullptr);
  std::vector<Literal> clause =
      ClosingClause(node, skip_unprocessed_implied_nodes);
  clause.push_back(DecodeWithIdentityMapping(implied));
  return clause;
}

std::vector<Literal> SharedTreeManager::ClosingClause(
    const Node* node, bool skip_unprocessed_implied_nodes) const {
  // This is only used for LRAT, which only works for pure SAT, without
  // presolve. In this case all workers should have the same identity mapping
  // from the proto variables.
  CHECK_NE(node, nullptr);
  std::vector<Literal> clause;
  while (node->parent != nullptr) {
    // When a node is implied its implications are moved to its first
    // non-implied ancestor, instead of to its parent. Proving this with the
    // clause that the node is implied requires the implication clauses to
    // exclude the decisions of implied nodes. And since the clause that a node
    // is implied is the closing clause of its sibling, closing clauses should
    // also exclude the decisions of implied nodes.
    const bool is_implied = node->implied && (node->implied_and_processed ||
                                              skip_unprocessed_implied_nodes);
    if (!is_implied) {
      clause.push_back(DecodeWithIdentityMapping(node->decision).Negated());
    }
    node = node->parent;
  }
  return clause;
}

namespace {
bool UnorderedSpansAreEqual(absl::Span<const Literal> a,
                            absl::Span<const Literal> b) {
  if (a.size() != b.size()) return false;
  std::vector<Literal> sorted_a(a.begin(), a.end());
  std::vector<Literal> sorted_b(b.begin(), b.end());
  std::sort(sorted_a.begin(), sorted_a.end());
  std::sort(sorted_b.begin(), sorted_b.end());
  return sorted_a == sorted_b;
}
}  // namespace

ClauseId SharedTreeManager::AddImportedAndInferredClauses(
    absl::Span<const Literal> imported_clause,
    absl::Span<const Literal> inferred_clause,
    std::vector<ClauseId>& lrat_proof) {
  const ClauseId id = clause_id_generator_.GetNextId();
  lrat_proof_handler_->AddImportedClause(id, imported_clause);
  if (!lrat_proof.empty() ||
      !UnorderedSpansAreEqual(inferred_clause, imported_clause)) {
    lrat_proof.push_back(id);
    const ClauseId new_id = clause_id_generator_.GetNextId();
    lrat_proof_handler_->AddInferredClause(new_id, inferred_clause, lrat_proof,
                                           /*exported=*/true);
    lrat_proof_handler_->DeleteClause(id, {});
    return new_id;
  } else {
    return id;
  }
}

void SharedTreeManager::AssignLeaf(ProtoTrail& path, Node* leaf) {
  path.Clear();
  std::vector<Node*> reversed_path;
  while (leaf != &nodes_[0]) {
    reversed_path.push_back(&nodes_[leaf->id - node_id_offset_]);
    leaf = leaf->parent;
  }
  while (!reversed_path.empty()) {
    Node* leaf = reversed_path.back();
    reversed_path.pop_back();
    path.PushLevel(leaf->decision, leaf->objective_lb, leaf->id);
    if (leaf->implied) {
      path.SetLevelImplied(path.MaxLevel());
    }
    if (params_.shared_tree_worker_enable_trail_sharing() &&
        leaf->trail_info != nullptr) {
      for (const auto& [var, lb_and_clause] : leaf->trail_info->implications) {
        const auto [lb, clause_id] = lb_and_clause;
        path.AddImplication(path.MaxLevel(), ProtoLiteral(var, lb));
      }
    }
  }
}

bool SharedTreeManager::IsValid(const ProtoTrail& path) const {
  auto node_ids = path.NodeIds(path.MaxLevel());
  if (node_ids.empty()) return true;
  if (node_ids.back() < node_id_offset_) return false;
  return true;
}

void SharedTreeManager::RestartLockHeld() {
  node_id_offset_ += nodes_.size();
  if (lrat_proof_handler_ != nullptr) {
    for (const Node& node : nodes_) {
      if (node.closing_clause_id != kNoClauseId) {
        lrat_proof_handler_->DeleteClause(node.closing_clause_id, {});
      }
    }
  }
  nodes_.resize(1);
  nodes_[0].id = node_id_offset_;
  nodes_[0].children = {nullptr, nullptr};
  unassigned_leaves_.clear();
  DCHECK(to_close_.empty());
  DCHECK(to_update_.empty());
  num_splits_wanted_ =
      num_workers_ * params_.shared_tree_open_leaves_per_worker() - 1;
  num_closed_nodes_ = 0;
  num_restarts_ += 1;
  num_leaves_assigned_since_restart_ = 0;
}

void SharedTreeManager::CloseLratProof() {
  absl::MutexLock l(mu_);
  if (lrat_proof_handler_ != nullptr) {
    lrat_proof_handler_->Close(/*model_is_unsat=*/false);
    lrat_proof_handler_.reset();
  }
}

std::string SharedTreeManager::ShortStatus() const {
  return absl::StrCat("shared_tree_manager(r=", num_restarts_,
                      " n=", nodes_.size(), ")");
}

namespace {
void CheckEqual(absl::Span<const Literal> a, absl::Span<const Literal> b) {
  std::vector<Literal> sorted_a(a.begin(), a.end());
  std::vector<Literal> sorted_b(b.begin(), b.end());
  std::sort(sorted_a.begin(), sorted_a.end());
  std::sort(sorted_b.begin(), sorted_b.end());
  CHECK_EQ(sorted_a, sorted_b);
}
}  // namespace

bool SharedTreeManager::CheckLratInvariants() const {
  if (lrat_proof_handler_ != nullptr &&
      lrat_proof_handler_->lrat_check_enabled()) {
    for (const Node& node : nodes_) {
      if (node.parent == nullptr) continue;
      if (node.closed) {
        CheckEqual(
            lrat_proof_handler_->GetLratClauseForDebug(node.closing_clause_id),
            ClosingClause(&node));
      }
      if (node.trail_info != nullptr) {
        for (const auto& [var, lb_and_clause] : node.trail_info->implications) {
          const auto [lb, clause_id] = lb_and_clause;
          CheckEqual(lrat_proof_handler_->GetLratClauseForDebug(clause_id),
                     ImplicationClause(&node, ProtoLiteral(var, lb)));
        }
      }
    }
  }
  return true;
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
      clause_id_generator_(model->GetOrCreate<ClauseIdGenerator>()),
      lrat_proof_handler_(model->Mutable<LratProofHandler>()),
      integer_trail_(model->GetOrCreate<IntegerTrail>()),
      encoder_(model->GetOrCreate<IntegerEncoder>()),
      objective_(model->Get<ObjectiveDefinition>()),
      random_(model->GetOrCreate<ModelRandomGenerator>()),
      helper_(model->GetOrCreate<IntegerSearchHelper>()),
      heuristics_(model->GetOrCreate<SearchHeuristics>()),
      decision_policy_(model->GetOrCreate<SatDecisionPolicy>()),
      restart_policy_(model->GetOrCreate<RestartPolicy>()),
      level_zero_callbacks_(model->GetOrCreate<LevelZeroCallbackHelper>()),
      reversible_int_repository_(model->GetOrCreate<RevIntRepository>()),
      assigned_tree_lbds_(/*window_size=*/8) {}

std::vector<Literal>& SharedTreeWorker::DecisionReason(int level) {
  CHECK_LE(level, assigned_tree_decisions_.size());
  reason_.clear();
  for (int i = 0; i < level; ++i) {
    reason_.push_back(assigned_tree_decisions_[i].Negated());
  }
  return reason_;
}

bool SharedTreeWorker::AddDecisionImplication(Literal lit, int level,
                                              ClauseId clause_id) {
  CHECK_GT(level, 0);
  CHECK_NE(lit.Index(), kNoLiteralIndex);
  CHECK(!sat_solver_->Assignment().LiteralIsTrue(lit));
  absl::Span<const Literal> reason = DecisionReason(level);
  if (sat_solver_->Assignment().LiteralIsFalse(lit)) {
    VLOG(2) << "Closing subtree via impl at " << level + 1
            << " assigned=" << assigned_tree_.MaxLevel();
    if (lrat_proof_handler_ != nullptr) {
      // Use the fact that `reason` implies both `lit` and not(`lit`) to prove
      // that the tree can be closed.
      const ClauseId closing_clause_id = clause_id_generator_->GetNextId();
      std::vector<ClauseId> clause_ids;
      clause_manager_->AppendClauseIdsFixing({lit}, &clause_ids);
      clause_ids.push_back(clause_id);
      lrat_proof_handler_->AddInferredClause(closing_clause_id,
                                             DecisionReason(level), clause_ids,
                                             /*exported=*/true);
      lrat_proof_handler_->DeleteClause(closing_clause_id, {});
    }
    trail_->MutableConflict()->assign(reason.begin(), reason.end());
    manager_->CloseTree(assigned_tree_, level);
    assigned_tree_decisions_.clear();
    return false;
  }
  VLOG(2) << "Learned shared clause";
  trail_->GetEmptyVectorToStoreReason()->assign(reason.begin(), reason.end());
  return trail_->EnqueueWithStoredReason(clause_id, lit);
}

bool SharedTreeWorker::AddImplications() {
  const int level = sat_solver_->CurrentDecisionLevel();
  // Level 0 implications are unit clauses and are synced elsewhere.
  if (level == 0) return false;
  if (level > assigned_tree_.MaxLevel()) {
    return false;
  }
  rev_num_processed_implications_.resize(level + 1, 0);
  auto& num_processed_implications = rev_num_processed_implications_[level];
  reversible_int_repository_->SaveState(&num_processed_implications);
  absl::Span<const std::pair<Literal, ClauseId>> implied_literals =
      absl::MakeConstSpan(assigned_tree_implications_[level - 1])
          .subspan(num_processed_implications);
  bool added_clause = false;
  for (const auto& [implied, clause_id] : implied_literals) {
    ++num_processed_implications;
    if (sat_solver_->Assignment().LiteralIsTrue(implied)) continue;
    added_clause = true;
    if (!AddDecisionImplication(implied, level, clause_id)) return true;
  }
  if (objective_ != nullptr &&
      objective_->objective_var != kNoIntegerVariable) {
    const IntegerValue obj_lb =
        integer_trail_->LowerBound(objective_->objective_var);
    assigned_tree_.SetObjectiveLb(level, obj_lb);
    const Literal obj_lit =
        encoder_->GetOrCreateAssociatedLiteral(IntegerLiteral::GreaterOrEqual(
            objective_->objective_var, assigned_tree_.ObjectiveLb(level)));
    if (!sat_solver_->Assignment().LiteralIsTrue(obj_lit)) {
      AddDecisionImplication(obj_lit, level, kNoClauseId);
      return true;
    }
  }
  DCHECK(CheckLratInvariants());
  return added_clause;
}

void SharedTreeWorker::ClearAssignedTreeDecisionsAndImplications() {
  // Delete all LRAT clauses corresponding to the assigned tree implications,
  // which are deleted too. Note that there is one LRAT proof per worker. Each
  // proof uses its local clause IDs, and there is no global clause ID space.
  // Individual proofs can be merged at the end of the solve, if UNSAT. In this
  // case clause deletions of individual proofs are ignored until the clause is
  // no longer needed by any other partial proof. Hence it is safe to delete the
  // clauses here, even if they are still needed in the SharedTreeManager.
  if (lrat_proof_handler_ != nullptr) {
    for (const auto& implications : assigned_tree_implications_) {
      for (const auto& [literal, clause_id] : implications) {
        lrat_proof_handler_->DeleteClause(clause_id, {});
      }
    }
  }
  assigned_tree_decisions_.clear();
  assigned_tree_implications_.clear();
}

bool SharedTreeWorker::SyncWithLocalTrail() {
  DCHECK(CheckLratInvariants());
  std::vector<int> new_implication_trail_indices;
  while (true) {
    if (lrat_proof_handler_ != nullptr) {
      trail_implication_clauses_.resize(reversible_trail_index_, kNoClauseId);
    }
    if (!sat_solver_->FinishPropagation()) return false;
    // Ensure we are at fixed point w.r.t. implications in the tree up to the
    // current level.
    if (AddImplications()) continue;

    if (!helper_->BeforeTakingDecision()) return false;
    const int level = sat_solver_->CurrentDecisionLevel();
    if (parameters_->shared_tree_worker_enable_trail_sharing() && level > 0 &&
        level <= assigned_tree_.MaxLevel() &&
        reversible_trail_index_ < trail_->Index()) {
      const int binary_propagator_id = binary_propagator_->PropagatorId();
      // Add implications from the local trail to share with other workers.
      reversible_int_repository_->SaveState(&reversible_trail_index_);
      new_implication_trail_indices.clear();
      for (int i = trail_->Index() - 1; i >= reversible_trail_index_; --i) {
        const Literal lit = (*trail_)[i];
        const int assignment_type = trail_->AssignmentType(lit.Variable());
        if (assignment_type == AssignmentType::kSearchDecision) break;
        // Avoid sharing implications from binary clauses - these are always
        // shared, so the implication will be propagated anyway.
        if (assignment_type == binary_propagator_id) continue;
        std::optional<ProtoLiteral> encoded = EncodeDecision(lit);
        if (!encoded.has_value()) continue;
        if (assigned_tree_.AddImplication(level, *encoded) &&
            lrat_proof_handler_ != nullptr) {
          new_implication_trail_indices.push_back(i);
        }
      }
      // Add LRAT inferred clauses for the new implications, so that other
      // workers can import them without proof. Do this in increasing trail
      // index order, and reuse the previously added clauses to prove the new
      // ones (to avoid a quadratic complexity).
      if (lrat_proof_handler_ != nullptr) {
        trail_implication_clauses_.resize(trail_->Index(), kNoClauseId);
        for (int i = new_implication_trail_indices.size() - 1; i >= 0; --i) {
          const int new_trail_index = new_implication_trail_indices[i];
          const Literal lit = (*trail_)[new_trail_index];
          trail_implication_clauses_[new_trail_index] =
              AddLratClauseAndProofForImplication(
                  lit, level, [&](int /*level*/, int trail_index) {
                    return trail_implication_clauses_[trail_index];
                  });
        }
      }
      reversible_trail_index_ = trail_->Index();
    }
    if (level >= assigned_tree_.MaxLevel()) break;
    // The next decision is assigned, make sure it makes sense.
    const Literal next_decision = assigned_tree_decisions_[level];
    if (!sat_solver_->Assignment().LiteralIsAssigned(next_decision)) break;
    if (sat_solver_->Assignment().LiteralIsFalse(next_decision)) {
      // Next assigned decision is impossible.
      VLOG(2) << "Closing subtree at " << level + 1
              << " assigned=" << assigned_tree_.MaxLevel();
      // Add the LRAT inferred clause "current decisions => not(next_decision)"
      // so that it can be imported in SharedTreeManager to close the tree. We
      // can delete it right away since we don't need it in the worker itself.
      const ClauseId clause_id =
          AddLratClauseAndProofForImplication(next_decision.Negated(), level);
      manager_->CloseTree(assigned_tree_, level + 1);
      if (lrat_proof_handler_ != nullptr) {
        lrat_proof_handler_->DeleteClause(clause_id, {});
      }
      ClearAssignedTreeDecisionsAndImplications();
      sat_solver_->Backtrack(0);
    } else {
      // The next level is implied by the current one.
      if (lrat_proof_handler_ != nullptr) {
        // Update the LRAT clause of each implied literal at any next level, in
        // order to remove `next_decision` from these implications. Each new
        // clause is proved with the old one, combined with the clause that the
        // current decisions imply the next one.
        const ClauseId clause_id =
            AddLratClauseAndProofForImplication(next_decision, level);
        std::vector<Literal> implication;
        for (int i = 0; i < level; ++i) {
          implication.push_back(assigned_tree_decisions_[i].Negated());
        }
        for (int l = level; l < assigned_tree_decisions_.size(); ++l) {
          if (l != level) {
            implication.push_back(assigned_tree_decisions_[l].Negated());
          }
          for (auto& [lit, id] : assigned_tree_implications_[l]) {
            const ClauseId old_id = id;
            id = clause_id_generator_->GetNextId();
            implication.push_back(lit);
            lrat_proof_handler_->AddInferredClause(
                id, implication, {clause_id, old_id}, /*exported=*/true);
            lrat_proof_handler_->DeleteClause(old_id, {});
            implication.pop_back();
          }
        }
        lrat_proof_handler_->DeleteClause(clause_id, {});
      }
      assigned_tree_.SetLevelImplied(level + 1);
      if (level > 0) {
        assigned_tree_implications_[level - 1].insert(
            assigned_tree_implications_[level - 1].end(),
            assigned_tree_implications_[level].begin(),
            assigned_tree_implications_[level].end());
      }
      assigned_tree_implications_.erase(assigned_tree_implications_.begin() +
                                        level);
      assigned_tree_decisions_.erase(assigned_tree_decisions_.begin() + level);
    }
  }
  DCHECK(CheckLratInvariants());
  return true;
}

ClauseId SharedTreeWorker::AddLratClauseAndProofForImplication(
    Literal literal, int level,
    std::optional<absl::FunctionRef<ClauseId(int, int)>> root_literals) {
  if (lrat_proof_handler_ == nullptr) return kNoClauseId;

  CHECK_LE(level, assigned_tree_decisions_.size());
  const ClauseId clause_id = clause_id_generator_->GetNextId();
  std::vector<Literal>& implication = DecisionReason(level);
  implication.push_back(literal);
  std::vector<ClauseId> clause_ids;
  clause_manager_->AppendClauseIdsFixing(
      {literal}, &clause_ids, /*decision=*/kNoLiteralIndex, root_literals);
  lrat_proof_handler_->AddInferredClause(clause_id, implication, clause_ids,
                                         /*exported=*/true);
  return clause_id;
}

ClauseId SharedTreeWorker::ImportLratClauseForImplication(Literal literal,
                                                          int level) {
  if (lrat_proof_handler_ == nullptr) return kNoClauseId;

  CHECK_LE(level, assigned_tree_decisions_.size());
  const ClauseId clause_id = clause_id_generator_->GetNextId();
  std::vector<Literal>& implication = DecisionReason(level);
  implication.push_back(literal);
  lrat_proof_handler_->AddImportedClause(clause_id, implication);
  return clause_id;
}

bool SharedTreeWorker::NextDecision(LiteralIndex* decision_index) {
  const auto& decision_policy =
      heuristics_->decision_policies[heuristics_->policy_index];
  const int next_level = sat_solver_->CurrentDecisionLevel() + 1;
  CHECK_EQ(assigned_tree_decisions_.size(), assigned_tree_.MaxLevel());
  if (next_level <= assigned_tree_.MaxLevel()) {
    VLOG(2) << "Following shared trail depth=" << next_level << " "
            << parameters_->name();
    const Literal decision = assigned_tree_decisions_[next_level - 1];
    CHECK(!sat_solver_->Assignment().LiteralIsFalse(decision))
        << " at depth " << next_level << " " << parameters_->name();
    CHECK(!sat_solver_->Assignment().LiteralIsTrue(decision));
    *decision_index = decision.Index();
    return true;
  }
  return helper_->GetDecision(decision_policy, decision_index);
}

void SharedTreeWorker::MaybeProposeSplits() {
  if (time_limit_->GetElapsedDeterministicTime() <= next_split_dtime_) {
    return;
  }
  next_split_dtime_ = time_limit_->GetElapsedDeterministicTime() +
                      parameters_->shared_tree_split_min_dtime();
  tmp_splits_.clear();
  const int max_split_level =
      std::min<int>(trail_->CurrentDecisionLevel(), manager_->MaxPathDepth());
  for (int i = assigned_tree_.MaxLevel(); i < max_split_level; ++i) {
    const Literal split_decision = trail_->Decisions()[i].literal;
    const std::optional<ProtoLiteral> encoded = EncodeDecision(split_decision);
    if (!encoded.has_value()) break;
    tmp_splits_.push_back(*encoded);
  }
  const int splits_accepted =
      manager_->TrySplitTree(tmp_splits_, assigned_tree_);
  for (int i = 0; i < splits_accepted; ++i) {
    assigned_tree_decisions_.push_back(DecodeDecision(tmp_splits_[i]));
    assigned_tree_implications_.push_back({});
  }
}

bool SharedTreeWorker::ShouldReplaceSubtree() {
  // If we have no assignment, try to get one.
  if (assigned_tree_.MaxLevel() == 0) return true;
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
  DCHECK_EQ(trail_->CurrentDecisionLevel(), 0);
  DCHECK(CheckLratInvariants());
  manager_->SyncTree(assigned_tree_);
  assigned_tree_.NormalizeImplications();
  if (ShouldReplaceSubtree()) {
    ++num_trees_;
    VLOG(2) << parameters_->name() << " acquiring tree #" << num_trees_
            << " after " << restart_policy_->NumRestarts() << " restarts"
            << " prev depth: " << assigned_tree_.MaxLevel()
            << " target: " << assigned_tree_lbds_.WindowAverage()
            << " lbd: " << restart_policy_->LbdAverageSinceReset();
    if (parameters_->shared_tree_worker_enable_phase_sharing() &&
        // Only save the phase if we've done a non-trivial amount of work on
        // this subtree.
        FinishedMinRestarts() &&
        !decision_policy_->GetBestPartialAssignment().empty()) {
      assigned_tree_.ClearTargetPhase();
      for (Literal lit : decision_policy_->GetBestPartialAssignment()) {
        // Skip anything assigned at level 0.
        if (trail_->Assignment().LiteralIsAssigned(lit)) continue;
        // If `lit` was last assigned at a shared level, it is implied in the
        // tree, no need to share its phase.
        if (trail_->Info(lit.Variable()).level <= assigned_tree_.MaxLevel()) {
          continue;
        }
        // Only set the phase for booleans to avoid creating literals on other
        // workers.
        auto encoded = ProtoLiteral::EncodeLiteral(lit, mapping_);
        if (!encoded.has_value()) continue;
        if (!assigned_tree_.AddPhase(*encoded)) break;
      }
    }
    manager_->ReplaceTree(assigned_tree_);
    assigned_tree_.NormalizeImplications();
    assigned_tree_lbds_.Add(restart_policy_->LbdAverageSinceReset());
    restart_policy_->Reset();
    earliest_replacement_dtime_ = 0;
    if (assigned_tree_.MaxLevel() > 0) {
      next_split_dtime_ = time_limit_->GetElapsedDeterministicTime() +
                          parameters_->shared_tree_split_min_dtime();
    }
    if (parameters_->shared_tree_worker_enable_phase_sharing()) {
      VLOG(2) << "Importing phase of length: "
              << assigned_tree_.TargetPhase().size();
      decision_policy_->ClearBestPartialAssignment();
      for (const ProtoLiteral& lit : assigned_tree_.TargetPhase()) {
        decision_policy_->SetTargetPolarityIfUnassigned(DecodeDecision(lit));
      }
      decision_policy_->ResetActivitiesToFollowBestPartialAssignment();
      // This seems bizzare after just setting the best partial assignment,
      // but this makes phase sharing work even when there is no stable phase in
      // the restart strategy, and makes no real difference if there is, since
      // the first dive will still try to follow this assignment until the first
      // conflict regardless of the restart strategy.
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
  VLOG(2) << "Assigned level: " << assigned_tree_.MaxLevel() << " "
          << parameters_->name();
  ClearAssignedTreeDecisionsAndImplications();
  for (int level = 1; level <= assigned_tree_.MaxLevel(); ++level) {
    assigned_tree_decisions_.push_back(
        DecodeDecision(assigned_tree_.Decision(level)));
    std::vector<std::pair<Literal, ClauseId>> implications;
    for (const ProtoLiteral& impl : assigned_tree_.Implications(level)) {
      const Literal lit = DecodeDecision(impl);
      implications.emplace_back(lit,
                                ImportLratClauseForImplication(lit, level));
    }
    assigned_tree_implications_.push_back(std::move(implications));
  }
  DCHECK(CheckLratInvariants());
  return true;
}

SatSolver::Status SharedTreeWorker::Search(
    const std::function<void()>& feasible_solution_observer) {
  // Inside GetAssociatedLiteral if a literal becomes fixed at level 0 during
  // Search, the code CHECKs it is at level 0 when decoding the literal, but
  // the fixed literals are cached, so we can create them now to avoid a
  // crash.
  sat_solver_->Backtrack(0);
  encoder_->GetTrueLiteral();
  encoder_->GetFalseLiteral();
  level_zero_callbacks_->callbacks.push_back(
      [this]() { return SyncWithSharedTree(); });
  const bool has_objective =
      objective_ != nullptr && objective_->objective_var != kNoIntegerVariable;
  while (!time_limit_->LimitReached()) {
    if (!sat_solver_->FinishPropagation()) {
      return sat_solver_->UnsatStatus();
    }
    if (heuristics_->restart_policies[heuristics_->policy_index]()) {
      heuristics_->policy_index = restart_policy_->NumRestarts() %
                                  heuristics_->decision_policies.size();
      sat_solver_->Backtrack(0);
    }
    if (!SyncWithLocalTrail()) return sat_solver_->UnsatStatus();
    LiteralIndex decision_index;
    if (!NextDecision(&decision_index)) continue;
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

      continue;
    }
    const Literal decision(decision_index);
    CHECK(!sat_solver_->Assignment().LiteralIsFalse(decision));
    CHECK(!sat_solver_->Assignment().LiteralIsTrue(decision));
    // The LRAT proofs assume that an assigned tree decision is the actual one
    // which is taken here.
    if (!helper_->TakeDecision(
            decision, /*use_representative=*/lrat_proof_handler_ == nullptr)) {
      return sat_solver_->UnsatStatus();
    }
    MaybeProposeSplits();
  }

  return SatSolver::LIMIT_REACHED;
}

Literal SharedTreeWorker::DecodeDecision(ProtoLiteral lit) {
  return lit.Decode(mapping_, encoder_);
}

std::optional<ProtoLiteral> SharedTreeWorker::EncodeDecision(Literal decision) {
  return ProtoLiteral::Encode(decision, mapping_, encoder_);
}

bool SharedTreeWorker::CheckLratInvariants() {
  if (lrat_proof_handler_ != nullptr &&
      lrat_proof_handler_->lrat_check_enabled()) {
    for (int level = 0; level < assigned_tree_decisions_.size(); ++level) {
      for (auto& [lit, id] : assigned_tree_implications_[level]) {
        std::vector<Literal>& expected = DecisionReason(level + 1);
        expected.push_back(lit);
        CheckEqual(lrat_proof_handler_->GetLratClauseForDebug(id), expected);
      }
    }
  }
  return true;
}

}  // namespace operations_research::sat
