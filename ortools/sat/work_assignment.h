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

#ifndef ORTOOLS_SAT_WORK_ASSIGNMENT_H_
#define ORTOOLS_SAT_WORK_ASSIGNMENT_H_

#include <stdint.h>
#include <sys/stat.h>

#include <array>
#include <deque>
#include <functional>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "absl/base/nullability.h"
#include "absl/base/thread_annotations.h"
#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/random/bit_gen_ref.h"
#include "absl/synchronization/mutex.h"
#include "absl/types/span.h"
#include "ortools/base/strong_vector.h"
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
#include "ortools/util/running_stat.h"
#include "ortools/util/strong_integers.h"
#include "ortools/util/time_limit.h"

namespace operations_research::sat {

class ProtoLiteral {
 public:
  ProtoLiteral() = default;
  ProtoLiteral(int var, IntegerValue lb) : proto_var_(var), lb_(lb) {}
  ProtoLiteral Negated() const {
    return ProtoLiteral(NegatedRef(proto_var_), -lb_ + 1);
  }
  int proto_var() const { return proto_var_; }
  IntegerValue lb() const { return lb_; }
  bool operator==(const ProtoLiteral& other) const {
    return proto_var_ == other.proto_var_ && lb_ == other.lb_;
  }
  bool operator!=(const ProtoLiteral& other) const { return !(*this == other); }
  template <typename H>
  friend H AbslHashValue(H h, const ProtoLiteral& literal) {
    return H::combine(std::move(h), literal.proto_var_, literal.lb_);
  }

 private:
  int proto_var_ = kint32max;
  IntegerValue lb_ = kMaxIntegerValue;
};

// Identifiers for nodes are assigned by the SharedTreeManager, siblings always
// have consecutive IDs (with the first child always being even, so `id ^ 1`
// gives the id of the sibling). Children always have NodeIds greater than their
// parent.
DEFINE_STRONG_INDEX_TYPE(NodeId);
static constexpr NodeId kNoNodeId = NodeId(std::numeric_limits<int>::max());

// Contains all the relevant information about a node in the search tree in a
// worker-independent format.
// Shared tree nodes' ids, decisions, and parents are logically immutable, so
// all workers will agree on them. This means that a node can become "implied"
// because its sibling node is closed, at which point the former decision will
// not truly be a decision any more, but is guaranteed to be true on the path to
// this node, and may still appear in reason clauses in descendant nodes.
// TODO(user): Maybe make this a proto.
struct SharedTreeNode {
  NodeId id = kNoNodeId;
  NodeId parent_id = kNoNodeId;

  ProtoLiteral decision = ProtoLiteral();
  IntegerValue objective_lb = kMinIntegerValue;
  // A node is implied if its sibling node is closed (and the node is not closed
  // itself). This means that `decision` is not a true decision any more in this
  // tree, but it might still be in other workers trees that have not  synced
  // with this one yet, so we must keep the node to be able to sync.
  bool is_implied = false;

  // Stores shared lower bounds of variables at this node.
  // Always empty in closed or implied nodes.
  absl::flat_hash_map<int, IntegerValue> var_lower_bounds;

  // Some convenience functions to make code more readable.
  bool is_root() const { return parent_id == kNoNodeId; }
  bool is_closed() const { return objective_lb > kMaxIntegerValue; }
  NodeId sibling_id() const { return NodeId(id.value() ^ 1); }
  bool LiteralIsImplied(ProtoLiteral literal) const {
    auto it = var_lower_bounds.find(literal.proto_var());
    if (it == var_lower_bounds.end()) return false;
    return literal.lb() <= it->second;
  }
};

// Translates SharedTreeNode information according to a worker's mapping.
// The mapping may be null, in which case this is the SharedTreeManager's copy.
// Workers trees contain a subset of the nodes in the manager: just those on the
// path to their assigned leaf.
class SharedTreeEncoder {
 public:
  // This class should only be mutated via the SharedTreeEncoder that owns it.
  class Node {
   public:
    // Disable copy and assignment to ensure pointer stability.
    Node(const Node&) = delete;
    Node& operator=(const Node&) = delete;

    Node(SharedTreeEncoder* encoder, NodeId id, ProtoLiteral decision,
         std::optional<Literal> decoded_decision, Node* parent);

    const SharedTreeNode& shared() const { return shared_; }
    Literal decoded_decision() const { return decoded_decision_; }
    Node* parent() const { return parent_node_; }
    Node* sibling() const { return sibling_node_; }
    Node* child(int index) const { return children_[index]; }
    bool is_leaf() const { return children_[0] == nullptr; }

    // Returns the last ancestor of this node that is not implied, i.e. the
    // first node at the same decision level as this node.
    Node* GetLevelStart();

    // Adds an implication to this node.
    // Returns true if this implication is new.
    bool AddImplication(ProtoLiteral literal);

    // Adds a reason clause for the given literal that can be inferred from
    // `proof`.
    void ExportInferredReasonClause(Literal literal,
                                    absl::Span<const ClausePtr> proof);

    // Sets the objective bound of this node.
    // This may also update the objective bounds of the ancestors of this node.
    void SetObjectiveLowerBound(IntegerValue objective_lb);

    // Returns true if literal is a decision or implication of any decision node
    // on the path from the root to this node.
    // This is not particularly efficient, mostly intended for DCHECKs.
    bool LiteralIsTrue(ProtoLiteral literal);

    // Release memory used by closed and implied nodes.
    void Cleanup();

    // Returns the negation of non-implied decisions on the path from the root
    // to this node. These are the literals expected in the reason for
    // propagating any implication at this node or, equivalently, the literals
    // expected in a clause that closes this node
    std::vector<Literal> NegatedDecisions() const;

    // Returns the literals expected in a clause that propagates `implied` at
    // this node.
    std::vector<Literal> ExpectedReasonClauseLiterals(Literal implied) const;

    // Returns the reason clauses for all implications at this node. These
    // will contain a subset of the negated decoded decisions on the path from
    // this node to the root including at least all non-implied nodes. This will
    // be empty if LRAT is not enabled
    const absl::flat_hash_map<Literal, ClausePtr>& reason_clauses() const {
      return reason_clauses_;
    }

    // Returns reason_clauses()[literal] if it exists, or kNullClausePtr
    // otherwise.
    ClausePtr ReasonClauseOrNull(Literal literal) const;

    // Returns the decision level of this node (i.e. the number of non-implied
    // ancestors it has).
    int GetLevel();

   private:
    // Only SharedTreeEncoder is allowed to mutate a Node.
    friend class SharedTreeEncoder;
    void set_sibling(Node* s) { sibling_node_ = s; }
    void set_child(Node* child) {
      children_[child->shared().id.value() & 1] = child;
    }

    SharedTreeNode shared_;
    Literal decoded_decision_ = Literal(kNoLiteralIndex);

    // If lrat is enabled, this will contain the reason clause for each
    // literal implied at this node.
    absl::flat_hash_map<Literal, ClausePtr> reason_clauses_;

    SharedTreeEncoder* encoder_ = nullptr;
    Node* parent_node_ = nullptr;
    Node* sibling_node_ = nullptr;
    mutable Node* level_start_ = nullptr;
    std::array<Node*, 2> children_ = {nullptr, nullptr};
  };

  explicit SharedTreeEncoder(LratProofHandler* lrat_proof_handler,
                             CpModelMapping* mapping = nullptr,
                             IntegerEncoder* integer_encoder = nullptr);
  ~SharedTreeEncoder() { Clear(); }

  int Size() const { return nodes_.size(); }
  int ClosedLeaves() const { return closed_leaves_; }
  int TotalLeaves() const { return (nodes_.size() + 1) / 2; }
  int OpenLeaves() const { return TotalLeaves() - ClosedLeaves(); }

  // Translates a ProtoLiteral into a Literal, this is guaranteed to succeed if
  // ProtoLiteral was returned by Encode on any worker.
  Literal Decode(ProtoLiteral literal);

  // Encodes a literal as a ProtoLiteral. This can encode literals that occur in
  // the proto model, and also integer bounds literals.
  // Not all Literals have a counterpart in the model proto, in which case this
  // will return std::nullopt.
  std::optional<ProtoLiteral> Encode(Literal literal);

  // As above, but will only encode literals that are boolean variables or their
  // negations (i.e. not integer bounds literals).
  std::optional<ProtoLiteral> EncodeLiteral(Literal);

  // Imports information from node and decodes it.
  // If LRAT is enabled, and any new implications are added to `node`, the
  // encoder will import a clause with the same literals as
  // `importable_reason_clauses[implied]`.
  void ImportNode(const SharedTreeNode& node,
                  const absl::flat_hash_map<Literal, ClausePtr>&
                      importable_reason_clauses = {});

  // Convenience function to add children to `parent`.
  void SplitNode(NodeId parent, ProtoLiteral decision, NodeId first_child);

  // Closes `node_id`, does not takes ownership of the proof clauses.
  void CloseNode(NodeId node_id, absl::Span<const ClausePtr> proof);

  // Syncs all nodes on the path from the root to `leaf_id` with `worker_tree`.
  // `leaf_id` must be valid in this tree, but not necessarily in `worker_tree`.
  void SyncNodesOnPath(NodeId leaf_id, SharedTreeEncoder& worker_tree);

  // Clears all nodes in this tree and deletes any non-unit reason clauses.
  void Clear();

  // Returns the DecodedNode with the given id.
  Node* absl_nonnull GetNode(NodeId node_id) const {
    return nodes_by_id_.at(node_id);
  }

  // Returns all non-implied nodes on the path from root to leaf.
  std::vector<Node*> GetLevelStartNodes(NodeId leaf_id);

  // Ensures that all reason clauses in `node` contain exactly the
  // non-implied decisions on the path from the root to `node`.
  void NormalizeReasonClauses(Node* node);

  // CHECK fails if any invariants are violated, or returns true otherwise.
  // Very slow, intended for use in DCHECKs only.
  bool CheckInvariants();

 private:
  // Ensures that a node with the given id exists. If not, creates it.
  Node* EnsureNodeExists(NodeId id, NodeId parent_id, ProtoLiteral decision);

  // Ensures that `node` will be closed after the next call to
  // ProcessNodeChanges.
  // It will either closes the first node at the same level as node and enqueue
  // its children to be closed later, or close the node itself if
  // `parent_is_closed` is true.
  // Will call ProcessImpliedNode on the newly implied non-closed sibling, if
  // any.
  void CloseNodeInternal(Node* node, absl::Span<const ClausePtr> proof,
                         bool parent_is_closed = false);

  // Ensures that the decision at node is implied at the start of its parent's
  // level, and marks the node as implied, merging the levels. Moves all
  // reason clauses and implications from this node to the new start of its
  // level.
  void ProcessImpliedNode(Node* node, ClausePtr decision_reason_clause);

  // Ensures that `node`'s ancestors' objective bounds are consistent when a new
  // bound has been proven for `node`.
  void ProcessNewObjectiveBound(Node* node);

  // Closes the children nodes closed by CloseNodeInternal until fixed point.
  void ProcessNodeChanges();

  // Merges information from `other` into `node`.
  // If LRAT is enabled, and any new implications are added to `node`, the
  // encoder will import a clause containing the literals in
  // `importable_reason_clauses[implication]`.
  void UpdateNode(
      const SharedTreeNode& other,
      const absl::flat_hash_map<Literal, ClausePtr>& importable_reason_clauses);

  // Sets tmp_proof_ to the sequence of clauses needed to propagate the
  // decisions of all implied nodes on the path from the root to `node`.
  void SetProofToPropagateImpliedNodes(Node* node_id);

  ClausePtr NewImportedClause(absl::Span<const Literal> literals);
  ClausePtr NewInferredClause(absl::Span<const Literal> literals,
                              absl::Span<const ClausePtr> proof);

  // Moves any new reason clauses from `implied_node` to the start of its
  // level. If the start of the level is the root, we only move reason
  // clauses for literals that are decisions of children of `implied_node` that
  // are implied at level 0.
  void MoveReasonClausesToLevelStart(Node* implied_node);

  // Deletes clause if it is non-unit.
  // `clause` must have been returned by NewImportedClause or NewInferredClause.
  void DeleteClause(ClausePtr clause);

  LratProofHandler* lrat_proof_handler_ = nullptr;
  CpModelMapping* mapping_ = nullptr;
  IntegerEncoder* integer_encoder_ = nullptr;
  std::deque<Node> nodes_;
  absl::flat_hash_map<NodeId, Node*> nodes_by_id_;

  int closed_leaves_ = 0;

  // Temporary variables used to maintain invariants.
  std::vector<Node*> to_update_;
  std::vector<Node*> to_close_;
  std::vector<ClausePtr> tmp_proof_;
};

// Thread-safe class for managing work assignments between workers.
// This API is intended to investigate Graeme Gange & Peter Stuckey's proposal
// "Scalable Parallelization of Learning Solvers".
class SharedTreeManager {
 public:
  explicit SharedTreeManager(Model* model);
  SharedTreeManager(const SharedTreeManager&) = delete;

  int NumWorkers() const { return num_workers_; }
  int NumNodes() const ABSL_LOCKS_EXCLUDED(mu_);
  int MaxPathDepth() const { return max_path_depth_; }

  // Syncs the state of path with the shared search tree.
  // Clears `encoder` if a restart has invalidated the leaf_id.
  // Returns false if the problem is infeasible.
  bool SyncTree(NodeId leaf_id, SharedTreeEncoder& encoder)
      ABSL_LOCKS_EXCLUDED(mu_);

  // Assigns a new leaf node to the worker and ensures all nodes on the path
  // are added to encoder.
  // `phase` is expected to be the target phase for the old leaf, and will be
  // filled with the phase for the new leaf.
  NodeId ReplaceTree(NodeId leaf_id, SharedTreeEncoder& encoder,
                     std::vector<ProtoLiteral>& phase);

  // Attempts to split the tree repeatedly with the given decisions.
  // `path` will be extended with the accepted splits, the opposite branches
  // will be added as unassigned leaves.
  // Returns the new leaf id assigned to the worker.
  NodeId TrySplitTree(NodeId parent, absl::Span<const ProtoLiteral> decisions,
                      SharedTreeEncoder& encoder) ABSL_LOCKS_EXCLUDED(mu_);

  void Restart() {
    absl::MutexLock l(mu_);
    RestartLockHeld();
  }

  void CloseLratProof();

 private:
  bool IsValid(NodeId leaf_id) const ABSL_EXCLUSIVE_LOCKS_REQUIRED(mu_) {
    return leaf_id != kNoNodeId && leaf_id >= root_;
  };

  NodeId TrySplitTreeLockHeld(NodeId parent, ProtoLiteral decision,
                              SharedTreeEncoder& encoder)
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(mu_);

  void ProcessNodeChanges() ABSL_EXCLUSIVE_LOCKS_REQUIRED(mu_);

  void RestartLockHeld() ABSL_EXCLUSIVE_LOCKS_REQUIRED(mu_);
  std::string ShortStatus() const ABSL_EXCLUSIVE_LOCKS_REQUIRED(mu_);

  mutable absl::Mutex mu_;
  const SatParameters& params_;
  const int num_workers_;
  const int max_path_depth_;
  SharedResponseManager* const shared_response_manager_;
  std::unique_ptr<LratProofHandler> lrat_proof_handler_;

  // Stores the nodes in the search tree.
  SharedTreeEncoder tree_ ABSL_GUARDED_BY(mu_);
  // Stores unassigned leaf nodes, along with their target phase.
  std::deque<std::pair<NodeId, std::vector<ProtoLiteral>>> unassigned_leaves_
      ABSL_GUARDED_BY(mu_);
  NodeId root_ ABSL_GUARDED_BY(mu_) = NodeId(1);
  NodeId next_node_id_ ABSL_GUARDED_BY(mu_) = NodeId(2);

  // We limit the total nodes generated per restart to cap the RAM usage and
  // communication overhead. If we exceed this, workers become portfolio
  // workers when no unassigned leaves are available.
  const int max_nodes_;
  int num_leaves_assigned_since_restart_ ABSL_GUARDED_BY(mu_) = 0;

  int64_t num_restarts_ ABSL_GUARDED_BY(mu_) = 0;
};

class SharedTreeWorker {
 public:
  explicit SharedTreeWorker(Model* model);
  SharedTreeWorker(const SharedTreeWorker&) = delete;
  SharedTreeWorker& operator=(const SharedTreeWorker&) = delete;

  SatSolver::Status Search(
      const std::function<void()>& feasible_solution_observer);

 private:
  const int kMaxPhaseSize = 256;
  IntegerValue CurrentObjectiveLowerBound() const {
    if (objective_ == nullptr ||
        objective_->objective_var == kNoIntegerVariable) {
      return kMinIntegerValue;
    }
    return integer_trail_->LowerBound(objective_->objective_var);
  }
  bool LeafIsClosed() const {
    return !assigned_nodes_.empty() &&
           assigned_nodes_.back()->shared().is_closed();
  }

  // Syncs tree_ with the shared tree manager, possibly acquires a new
  // assignment, enqueues assumptions and finishes propagation.
  // Returns false if the problem is infeasible.
  bool SyncWithSharedTree();

  void MaybeProposeSplits();
  bool ShouldReplaceSubtree();
  bool FinishedMinRestarts() const {
    return tree_.Size() > 0 &&
           restart_policy_->NumRestarts() >=
               parameters_->shared_tree_worker_min_restarts_per_subtree();
  }

  // Returns the node fixing `literal` to true, or nullptr if it is not fixed by
  // a node in the assigned subtree. Will always return the start of a level.
  // This is safe to call interleaved with decoding ProtoLiterals, and on
  // literals fixed at the root, unlike raw access to fixed_by_.
  SharedTreeEncoder::Node* FixedByOrNull(Literal literal);

  // Exports any new implications to the shared tree.
  // Updates fixed_by_ for literals on the trail up until the first
  // decision not fixed by some node in the assigned subtree as it goes.
  void ExportNewImplications();

  // Returns a sequence of clauses sufficient to propagate `literal` using only
  // the decisions on the path from the root to `implied_by` as assumptions.
  // All literals must be assigned.
  absl::Span<const ClausePtr> GetProof(SharedTreeEncoder::Node* implied_by,
                                       Literal literal);

  // Sets sat_solver_'s assumptions to those on the path to `leaf_id`,
  // propagates them, and adds any new implications to tree_.
  // If there is a conflict in the assumptions it will close the appropriate
  // node, if any node's decision is propagated, it will close the appropriate
  // sibling node.
  // Returns false if the problem is infeasible.
  bool ResetAndEnqueueAssumptions(NodeId leaf_id);

  // Processes a conflict at or before the assumption level, closing the
  // appropriate node in the shared tree. Returns false if the problem is UNSAT.
  bool ProcessAssumptionConflict();

  // Closes the appropriate node in the shared tree  after either failing to
  // enqueue a decision, or after processing a conflict at the assumption
  // level, the solver will be reset to level 0.
  // Returns false if the problem is UNSAT.
  bool CloseNodeAfterConflict();

  SatParameters* parameters_;
  SharedResponseManager* shared_response_;
  TimeLimit* time_limit_;
  SharedTreeManager* manager_;
  CpModelMapping* mapping_;
  SatSolver* sat_solver_;
  Trail* trail_;
  BinaryImplicationGraph* binary_propagator_;
  ClauseManager* clause_manager_;
  LratProofHandler* lrat_proof_handler_;
  IntegerTrail* integer_trail_;
  IntegerEncoder* encoder_;
  const ObjectiveDefinition* objective_;
  absl::BitGenRef random_;
  IntegerSearchHelper* helper_;
  SearchHeuristics* heuristics_;
  SatDecisionPolicy* decision_policy_;
  RestartPolicy* restart_policy_;
  RevIntRepository* reversible_int_repository_;

  int64_t num_trees_ = 0;

  SharedTreeEncoder tree_;
  // Stores the first node at which each literal is made true.
  util_intops::StrongVector<LiteralIndex, SharedTreeEncoder::Node*> fixed_by_;
  // Stores the non-implied nodes in the assigned subtree.
  std::vector<SharedTreeEncoder::Node*> assigned_nodes_;
  NodeId leaf_id_ = kNoNodeId;
  std::vector<ProtoLiteral> phase_;

  double next_split_dtime_ = 0;

  std::vector<ProtoLiteral> tmp_splits_;
  // Stores the average LBD of learned clauses for each tree assigned since it
  // was assigned.
  // If a tree has worse LBD than the average over the last few trees we
  // replace the tree.
  RunningAverage assigned_tree_lbds_;
  double earliest_replacement_dtime_ = 0;

  // Stores the end of the literals implied by the assigned tree.
  int reversible_trail_index_ = 0;

  std::vector<ClausePtr> tmp_proof_clauses_;
};

}  // namespace operations_research::sat

#endif  // ORTOOLS_SAT_WORK_ASSIGNMENT_H_
