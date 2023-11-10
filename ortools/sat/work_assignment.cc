// Copyright 2010-2022 Google LLC
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
#include <cstdint>
#include <deque>
#include <functional>
#include <limits>
#include <optional>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "absl/log/check.h"
#include "absl/random/distributions.h"
#include "absl/strings/str_cat.h"
#include "absl/synchronization/mutex.h"
#include "absl/types/span.h"
#include "ortools/base/logging.h"
#include "ortools/sat/cp_model_mapping.h"
#include "ortools/sat/cp_model_utils.h"
#include "ortools/sat/integer.h"
#include "ortools/sat/integer_search.h"
#include "ortools/sat/model.h"
#include "ortools/sat/sat_base.h"
#include "ortools/sat/sat_parameters.pb.h"
#include "ortools/sat/sat_solver.h"
#include "ortools/sat/synchronization.h"
#include "ortools/sat/util.h"
#include "ortools/util/strong_integers.h"
#include "ortools/util/time_limit.h"

namespace operations_research::sat {
namespace {

// We restart the shared tree 10 times after 2 restarts per worker. After that
// we restart when the tree reaches the maximum allowable number of nodes, but
// still at most once per 2 restarts per worker.
const int kSyncsPerWorkerPerRestart = 2;
const int kNumInitialRestarts = 10;

// If you build a tree by expanding the nodes with minimal depth+discrepancy,
// the number of leaves when all nodes with a given value have been split
// follows the fibonacci sequence:
// num_leaves(0) := 2;
// num_leaves(1) := 3;
// num_leaves(n) := num_leaves(n-1) + num_leaves(n-2)
// This function returns f(n) := min({i | num_leaves(i) >= n})
int MaxAllowedDiscrepancyPlusDepth(int num_leaves) {
  int i = 0;
  int a = 1;
  int b = 2;
  while (b < num_leaves) {
    std::tie(a, b) = std::make_pair(b, a + b);
    ++i;
  }
  return i;
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
  if (literal.Index() == kNoLiteralIndex) {
    return std::nullopt;
  }
  int model_var =
      mapping->GetProtoVariableFromBooleanVariable(literal.Variable());
  if (model_var != -1) {
    CHECK(mapping->IsBoolean(model_var));
    ProtoLiteral result{
        literal.IsPositive() ? model_var : NegatedRef(model_var),
        literal.IsPositive() ? 1 : 0};
    DCHECK_EQ(result.Decode(mapping, encoder), literal);
    DCHECK_EQ(result.Negated().Decode(mapping, encoder), literal.Negated());
    return result;
  }
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

void ProtoTrail::PushLevel(const ProtoLiteral& decision,
                           IntegerValue objective_lb, int node_id) {
  CHECK_GT(node_id, 0);
  decision_indexes_.push_back(literals_.size());
  literals_.push_back(decision);
  node_ids_.push_back(node_id);
  if (!level_to_objective_lbs_.empty()) {
    objective_lb = std::max(level_to_objective_lbs_.back(), objective_lb);
  }
  level_to_objective_lbs_.push_back(objective_lb);
}

void ProtoTrail::SetLevelImplied(int level) {
  DCHECK_GE(level, 1);
  DCHECK_LE(level, decision_indexes_.size());
  SetObjectiveLb(level - 1, ObjectiveLb(level));
  decision_indexes_.erase(decision_indexes_.begin() + level - 1);
  level_to_objective_lbs_.erase(level_to_objective_lbs_.begin() + level - 1);
}

void ProtoTrail::Clear() {
  decision_indexes_.clear();
  literals_.clear();
  level_to_objective_lbs_.clear();
  node_ids_.clear();
}

void ProtoTrail::SetObjectiveLb(int level, IntegerValue objective_lb) {
  if (level == 0) return;
  level_to_objective_lbs_[level - 1] =
      std::max(objective_lb, level_to_objective_lbs_[level - 1]);
}

absl::Span<const int> ProtoTrail::NodeIds(int level) const {
  DCHECK_LE(level, decision_indexes_.size());
  int start = level == 0 ? 0 : decision_indexes_[level - 1];
  int end = level == decision_indexes_.size() ? node_ids_.size()
                                              : decision_indexes_[level];
  return absl::MakeSpan(node_ids_.data() + start, end - start);
}

absl::Span<const ProtoLiteral> ProtoTrail::Implications(int level) const {
  if (level > decision_indexes_.size()) {
    return absl::MakeSpan(literals_.data(), 0);
  }
  int start = level == 0 ? 0 : decision_indexes_[level - 1] + 1;
  int end = level == decision_indexes_.size() ? node_ids_.size()
                                              : decision_indexes_[level];
  return absl::MakeSpan(literals_.data() + start, end - start);
}

SharedTreeManager::SharedTreeManager(Model* model)
    : params_(*model->GetOrCreate<SatParameters>()),
      num_workers_(std::max(1, params_.shared_tree_num_workers())),
      shared_response_manager_(model->GetOrCreate<SharedResponseManager>()),
      num_splits_wanted_(num_workers_ - 1),
      max_nodes_(params_.shared_tree_max_nodes_per_worker() >=
                         std::numeric_limits<int>::max() / num_workers_
                     ? std::numeric_limits<int>::max()
                     : num_workers_ *
                           params_.shared_tree_max_nodes_per_worker()) {
  // Create the root node with a fake literal.
  nodes_.push_back(
      {.literal = ProtoLiteral(),
       .objective_lb =
           shared_response_manager_->GetInnerObjectiveLowerBound()});
  unassigned_leaves_.reserve(num_workers_);
  unassigned_leaves_.push_back(&nodes_.back());
}

int SharedTreeManager::NumNodes() const {
  absl::MutexLock mutex_lock(&mu_);
  return nodes_.size();
}

int SharedTreeManager::SplitsToGeneratePerWorker() const {
  absl::MutexLock mutex_lock(&mu_);
  return std::min<int>(num_splits_wanted_,
                       max_nodes_ - static_cast<int>(nodes_.size()));
}

bool SharedTreeManager::SyncTree(ProtoTrail& path) {
  absl::MutexLock mutex_lock(&mu_);
  std::vector<std::pair<Node*, int>> nodes = GetAssignedNodes(path);
  if (!IsValid(path)) {
    path.Clear();
    return false;
  }
  // We don't rely on these being empty, but we expect them to be.
  DCHECK(to_close_.empty());
  DCHECK(to_update_.empty());
  int prev_level = -1;
  for (const auto& [node, level] : nodes) {
    if (level == prev_level) {
      to_close_.push_back(GetSibling(node));
    }
    if (level > 0 && node->objective_lb < path.ObjectiveLb(level)) {
      node->objective_lb = path.ObjectiveLb(level);
      to_update_.push_back(node->parent);
    }
    prev_level = level;
  }
  ProcessNodeChanges();
  if (nodes.back().first->closed) {
    path.Clear();
    return false;
  }
  // Restart after processing updates - we might learn a new objective bound.
  if (++num_syncs_since_restart_ / num_workers_ > kSyncsPerWorkerPerRestart &&
      (num_restarts_ < kNumInitialRestarts || nodes_.size() >= max_nodes_)) {
    RestartLockHeld();
    path.Clear();
    return false;
  }
  // Sync lower bounds and implications from the shared tree to `path`.
  AssignLeaf(path, nodes.back().first);
  return true;
}

void SharedTreeManager::ProposeSplit(ProtoTrail& path, ProtoLiteral decision) {
  absl::MutexLock mutex_lock(&mu_);
  if (!IsValid(path)) return;
  std::vector<std::pair<Node*, int>> nodes = GetAssignedNodes(path);
  if (nodes.back().first->children[0] != nullptr) {
    LOG_IF(WARNING, nodes.size() > 1)
        << "Cannot resplit previously split node @ " << nodes.back().second
        << "/" << nodes.size();
    return;
  }
  if (nodes_.size() >= max_nodes_) {
    VLOG(1) << "Too many nodes to accept split";
    return;
  }
  if (num_splits_wanted_ <= 0) {
    VLOG(1) << "Enough splits for now";
    return;
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
    if (discrepancy + path.MaxLevel() >
        MaxAllowedDiscrepancyPlusDepth(num_workers_)) {
      VLOG(1) << "Too high discrepancy to accept split";
      return;
    }
  } else if (params_.shared_tree_split_strategy() ==
             SatParameters::SPLIT_STRATEGY_OBJECTIVE_LB) {
    if (nodes.back().first->objective_lb > nodes.front().first->objective_lb) {
      VLOG(1) << "Can only split nodes with minimum objective lb, "
              << nodes.back().first->objective_lb << " > "
              << nodes.front().first->objective_lb;
      return;
    }
  } else if (params_.shared_tree_split_strategy() ==
             SatParameters::SPLIT_STRATEGY_BALANCED_TREE) {
    if (path.MaxLevel() + 1 > log2(num_workers_)) {
      VLOG(1) << "Tree too unbalanced to accept split";
      return;
    }
  }
  VLOG_EVERY_N(1, 10) << unassigned_leaves_.size() << " unassigned leaves, "
                      << nodes_.size() << " subtrees, " << num_splits_wanted_
                      << " splits wanted";
  Split(nodes, decision);
  auto [new_leaf, level] = nodes.back();
  path.PushLevel(new_leaf->literal, new_leaf->objective_lb, new_leaf->id);
}

void SharedTreeManager::ReplaceTree(ProtoTrail& path) {
  absl::MutexLock mutex_lock(&mu_);
  std::vector<std::pair<Node*, int>> nodes = GetAssignedNodes(path);
  if (nodes.back().first->children[0] == nullptr &&
      !nodes.back().first->closed && nodes.size() > 1) {
    VLOG(1) << "Returning leaf to be replaced";
    unassigned_leaves_.push_back(nodes.back().first);
  }
  path.Clear();
  while (!unassigned_leaves_.empty()) {
    const int i = num_leaves_assigned_++ % unassigned_leaves_.size();
    std::swap(unassigned_leaves_[i], unassigned_leaves_.back());
    Node* leaf = unassigned_leaves_.back();
    unassigned_leaves_.pop_back();
    if (!leaf->closed && leaf->children[0] == nullptr) {
      AssignLeaf(path, leaf);
      return;
    }
  }
  VLOG(1) << "Assigning root because no unassigned leaves are available";
  // TODO(user): Investigate assigning a random leaf so workers can still
  // improve shared tree bounds.
}

SharedTreeManager::Node* SharedTreeManager::GetSibling(Node* node) {
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
  nodes.push_back(std::make_pair(parent->children[0], level + 1));
  unassigned_leaves_.push_back(parent->children[1]);
  --num_splits_wanted_;
}

SharedTreeManager::Node* SharedTreeManager::MakeSubtree(Node* parent,
                                                        ProtoLiteral literal) {
  nodes_.push_back(
      Node{.literal = literal,
           .objective_lb = parent->objective_lb,
           .parent = parent,
           .id = static_cast<int>(nodes_.size() + node_id_offset_)});
  return &nodes_.back();
}

void SharedTreeManager::ProcessNodeChanges() {
  while (!to_close_.empty()) {
    Node* node = to_close_.back();
    CHECK_NE(node, nullptr);
    to_close_.pop_back();
    // Iterate over open parents while each sibling is closed.
    while (node != nullptr && !node->closed) {
      node->closed = true;
      node->objective_lb = kMaxIntegerValue;
      // If we are closing a leaf, try to maintain the same number of leaves;
      num_splits_wanted_ += (node->children[0] == nullptr);
      for (Node* child : node->children) {
        if (child == nullptr || child->closed) continue;
        to_close_.push_back(child);
      }
      Node* sibling = GetSibling(node);
      if (sibling != nullptr) {
        sibling->implied = true;
        if (!sibling->closed) break;
      }
      node = node->parent;
    }
    DCHECK(node == nullptr || node->closed);
    if (node == nullptr) {
      shared_response_manager_->NotifyThatImprovingProblemIsInfeasible(
          ShortStatus());
    } else {
      to_update_.push_back(node->parent);
    }
  }
  bool root_updated = false;
  while (!to_update_.empty()) {
    Node* node = to_update_.back();
    to_update_.pop_back();
    // Iterate over parents while the lower bound can be improved.
    while (node != nullptr) {
      DCHECK(node->children[0] != nullptr);
      DCHECK(node->children[1] != nullptr);
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
  absl::MutexLock mutex_lock(&mu_);
  const int node_id_to_close = path.NodeIds(level).front();
  path.Clear();
  if (node_id_to_close < node_id_offset_) return;
  Node* node = &nodes_[node_id_to_close - node_id_offset_];
  VLOG(1) << "Closing subtree at level " << level;
  DCHECK(to_close_.empty());
  to_close_.push_back(node);
  ProcessNodeChanges();
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
    path.PushLevel(leaf->literal, leaf->objective_lb, leaf->id);
    if (leaf->implied) {
      path.SetLevelImplied(path.MaxLevel());
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
  nodes_.resize(1);
  nodes_[0].id = node_id_offset_;
  nodes_[0].children = {nullptr, nullptr};
  unassigned_leaves_.clear();
  num_splits_wanted_ = num_workers_ - 1;
  num_restarts_ += 1;
  num_syncs_since_restart_ = 0;
}

std::string SharedTreeManager::ShortStatus() const {
  return absl::StrCat("shared_tree_manager(r=", num_restarts_,
                      " n=", nodes_.size(), ")");
}

SharedTreeWorker::SharedTreeWorker(Model* model)
    : parameters_(model->GetOrCreate<SatParameters>()),
      shared_response_(model->GetOrCreate<SharedResponseManager>()),
      time_limit_(model->GetOrCreate<TimeLimit>()),
      manager_(model->GetOrCreate<SharedTreeManager>()),
      mapping_(model->GetOrCreate<CpModelMapping>()),
      sat_solver_(model->GetOrCreate<SatSolver>()),
      trail_(model->GetOrCreate<Trail>()),
      integer_trail_(model->GetOrCreate<IntegerTrail>()),
      encoder_(model->GetOrCreate<IntegerEncoder>()),
      objective_(model->Get<ObjectiveDefinition>()),
      random_(model->GetOrCreate<ModelRandomGenerator>()),
      helper_(model->GetOrCreate<IntegerSearchHelper>()),
      heuristics_(model->GetOrCreate<SearchHeuristics>()) {}

const std::vector<Literal>& SharedTreeWorker::DecisionReason(int level) {
  CHECK_LE(level, assigned_tree_literals_.size());
  reason_.clear();
  for (int i = 0; i < level; ++i) {
    reason_.push_back(assigned_tree_literals_[i].Negated());
  }
  return reason_;
}

bool SharedTreeWorker::AddDecisionImplication(Literal lit, int level) {
  CHECK_NE(lit.Index(), kNoLiteralIndex);
  CHECK(!sat_solver_->Assignment().LiteralIsTrue(lit));
  if (sat_solver_->Assignment().LiteralIsFalse(lit)) {
    VLOG(1) << "Closing subtree via impl at " << level + 1
            << " assigned=" << assigned_tree_.MaxLevel();
    integer_trail_->ReportConflict(DecisionReason(level), {});
    manager_->CloseTree(assigned_tree_, level);
    assigned_tree_literals_.clear();
    return false;
  }
  integer_trail_->EnqueueLiteral(lit, DecisionReason(level), {});
  VLOG(1) << "Learned shared clause";
  return true;
}

bool SharedTreeWorker::AddImplications(
    absl::Span<const ProtoLiteral> implied_literals) {
  const int level = sat_solver_->CurrentDecisionLevel();
  // Level 0 implications are unit clauses and are synced elsewhere.
  if (level == 0) return false;
  if (level > assigned_tree_.MaxLevel()) {
    return false;
  }
  bool added_clause = false;
  for (const ProtoLiteral& impl : implied_literals) {
    Literal lit(DecodeDecision(impl));
    if (sat_solver_->Assignment().LiteralIsTrue(lit)) continue;
    added_clause = true;
    if (!AddDecisionImplication(lit, level)) return true;
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
      AddDecisionImplication(obj_lit, level);
      return true;
    }
  }
  return added_clause;
}

bool SharedTreeWorker::SyncWithLocalTrail() {
  while (true) {
    if (!sat_solver_->FinishPropagation()) return false;
    // Ensure we are at fixed point w.r.t. implications in the tree up to the
    // current level.
    if (AddImplications(
            assigned_tree_.Implications(sat_solver_->CurrentDecisionLevel()))) {
      continue;
    }
    if (!helper_->BeforeTakingDecision()) return false;
    const int level = sat_solver_->CurrentDecisionLevel();
    if (level >= assigned_tree_.MaxLevel()) break;
    if (level == assigned_tree_.MaxLevel()) break;
    // The next decision is assigned, make sure it makes sense.
    const Literal next_decision = assigned_tree_literals_[level];
    if (!sat_solver_->Assignment().LiteralIsAssigned(next_decision)) break;
    if (sat_solver_->Assignment().LiteralIsFalse(next_decision)) {
      // Next assigned decision is impossible.
      VLOG(1) << "Closing subtree at " << level + 1
              << " assigned=" << assigned_tree_.MaxLevel();
      manager_->CloseTree(assigned_tree_, level + 1);
      assigned_tree_literals_.clear();
    } else {
      // The next level is implied by the current one.
      assigned_tree_.SetLevelImplied(level + 1);
      assigned_tree_literals_.erase(assigned_tree_literals_.begin() + level);
    }
  }
  return true;
}

bool SharedTreeWorker::NextDecision(LiteralIndex* decision_index) {
  const auto& decision_policy =
      heuristics_->decision_policies[heuristics_->policy_index];
  const int next_level = sat_solver_->CurrentDecisionLevel() + 1;
  CHECK_EQ(assigned_tree_literals_.size(), assigned_tree_.MaxLevel());
  if (next_level <= assigned_tree_.MaxLevel()) {
    VLOG(1) << "Following shared trail depth=" << next_level << " "
            << parameters_->name();
    const Literal decision = assigned_tree_literals_[next_level - 1];
    CHECK(!sat_solver_->Assignment().LiteralIsFalse(decision))
        << " at depth " << next_level << " " << parameters_->name();
    CHECK(!sat_solver_->Assignment().LiteralIsTrue(decision));
    *decision_index = decision.Index();
    return true;
  }
  if (objective_ == nullptr ||
      objective_->objective_var == kNoIntegerVariable) {
    return helper_->GetDecision(decision_policy, decision_index);
  }
  // If the current node is close to the global lower bound, maybe try to
  // improve it.
  const IntegerValue root_obj_lb =
      integer_trail_->LevelZeroLowerBound(objective_->objective_var);
  const IntegerValue root_obj_ub =
      integer_trail_->LevelZeroUpperBound(objective_->objective_var);
  const IntegerValue obj_split =
      root_obj_lb + absl::LogUniform<int64_t>(
                        *random_, 0, (root_obj_ub - root_obj_lb).value());
  const double objective_split_probability =
      parameters_->shared_tree_worker_objective_split_probability();
  return helper_->GetDecision(
      [&]() -> BooleanOrIntegerLiteral {
        IntegerValue obj_lb =
            integer_trail_->LowerBound(objective_->objective_var);
        IntegerValue obj_ub =
            integer_trail_->UpperBound(objective_->objective_var);
        if (obj_lb > obj_split || obj_ub <= obj_split ||
            next_level > assigned_tree_.MaxLevel() + 1 ||
            absl::Bernoulli(*random_, 1 - objective_split_probability)) {
          return decision_policy();
        }
        return BooleanOrIntegerLiteral(
            IntegerLiteral::LowerOrEqual(objective_->objective_var, obj_split));
      },
      decision_index);
}

void SharedTreeWorker::MaybeProposeSplit() {
  if (splits_wanted_ == 0 ||
      sat_solver_->CurrentDecisionLevel() != assigned_tree_.MaxLevel() + 1) {
    return;
  }
  const Literal split_decision =
      sat_solver_->Decisions()[assigned_tree_.MaxLevel()].literal;
  const std::optional<ProtoLiteral> encoded = EncodeDecision(split_decision);
  if (encoded.has_value()) {
    CHECK_EQ(assigned_tree_literals_.size(), assigned_tree_.MaxLevel());
    manager_->ProposeSplit(assigned_tree_, *encoded);
    if (assigned_tree_.MaxLevel() > assigned_tree_literals_.size()) {
      assigned_tree_literals_.push_back(split_decision);
      --splits_wanted_;
      CHECK_EQ(assigned_tree_literals_.size(), assigned_tree_.MaxLevel());
    }
    CHECK_EQ(assigned_tree_literals_.size(), assigned_tree_.MaxLevel());
  }
}

void SharedTreeWorker::SyncWithSharedTree() {
  splits_wanted_ = manager_->SplitsToGeneratePerWorker();
  VLOG(1) << "Splits wanted: " << splits_wanted_ << " " << parameters_->name();
  manager_->SyncTree(assigned_tree_);
  // If we have no assignment, try to get one.
  // We also want to ensure unassigned nodes have their lower bounds bumped
  // periodically, so workers need to occasionally replace open trees, but only
  // at most once per restart.
  // TODO(user): Ideally we should use some metric to replace a
  // subtree when the worker is doing badly.
  if (assigned_tree_.MaxLevel() == 0 ||
      (tree_assignment_restart_ < num_restarts_ &&
       absl::Bernoulli(*random_, 1e-2))) {
    manager_->ReplaceTree(assigned_tree_);
    tree_assignment_restart_ = num_restarts_;
  }
  VLOG(1) << "Assigned level: " << assigned_tree_.MaxLevel() << " "
          << parameters_->name();
  assigned_tree_literals_.clear();
  for (int i = 1; i <= assigned_tree_.MaxLevel(); ++i) {
    assigned_tree_literals_.push_back(
        DecodeDecision(assigned_tree_.Decision(i)));
  }
}

SatSolver::Status SharedTreeWorker::Search(
    const std::function<void()>& feasible_solution_observer) {
  // Inside GetAssociatedLiteral if a literal becomes fixed at level 0 during
  // Search,the code checks it is at level 0 when decoding the literal, but
  // the fixed literals are cached, so we can create them now to avoid a
  // crash.
  sat_solver_->Backtrack(0);
  encoder_->GetTrueLiteral();
  encoder_->GetFalseLiteral();
  const bool has_objective =
      objective_ != nullptr && objective_->objective_var != kNoIntegerVariable;
  std::vector<Literal> clause;
  while (!time_limit_->LimitReached()) {
    if (!sat_solver_->FinishPropagation()) {
      return sat_solver_->UnsatStatus();
    }
    if (heuristics_->restart_policies[heuristics_->policy_index]()) {
      ++num_restarts_;
      heuristics_->policy_index =
          num_restarts_ % heuristics_->decision_policies.size();
      sat_solver_->Backtrack(0);
    }
    if (trail_->CurrentDecisionLevel() == 0) {
      SyncWithSharedTree();
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
    if (!helper_->TakeDecision(decision)) {
      return sat_solver_->UnsatStatus();
    }
    MaybeProposeSplit();
  }

  return SatSolver::LIMIT_REACHED;
}

Literal SharedTreeWorker::DecodeDecision(ProtoLiteral lit) {
  return lit.Decode(mapping_, encoder_);
}

std::optional<ProtoLiteral> SharedTreeWorker::EncodeDecision(Literal decision) {
  return ProtoLiteral::Encode(decision, mapping_, encoder_);
}

}  // namespace operations_research::sat
