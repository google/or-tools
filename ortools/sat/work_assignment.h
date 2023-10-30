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

#ifndef OR_TOOLS_SAT_WORK_ASSIGNMENT_H_
#define OR_TOOLS_SAT_WORK_ASSIGNMENT_H_

#include <stdint.h>

#include <array>
#include <deque>
#include <functional>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "absl/base/thread_annotations.h"
#include "absl/log/check.h"
#include "absl/synchronization/mutex.h"
#include "absl/types/span.h"
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

class ProtoLiteral {
 public:
  ProtoLiteral() = default;
  ProtoLiteral(int var, IntegerValue lb) : proto_var_(var), lb_(lb) {}
  ProtoLiteral Negated() const {
    return ProtoLiteral(NegatedRef(proto_var_), -lb_ + 1);
  }
  bool operator==(const ProtoLiteral& other) const {
    return proto_var_ == other.proto_var_ && lb_ == other.lb_;
  }
  bool operator!=(const ProtoLiteral& other) const { return !(*this == other); }
  Literal Decode(CpModelMapping*, IntegerEncoder*) const;
  static std::optional<ProtoLiteral> Encode(Literal, CpModelMapping*,
                                            IntegerEncoder*);

 private:
  IntegerLiteral DecodeInteger(CpModelMapping*) const;
  static std::optional<ProtoLiteral> EncodeInteger(IntegerLiteral,
                                                   CpModelMapping*);

  int proto_var_ = std::numeric_limits<int>::max();
  IntegerValue lb_ = kMaxIntegerValue;
};

// ProtoTrail acts as an intermediate datastructure that can be synced
// with the shared tree and the local Trail as appropriate.
// It's intended that you sync a ProtoTrail with the tree on restart or when
// a subtree is closed, and with the local trail after propagation.
// Specifically it stores objective lower bounds, and literals that have been
// branched on and later proven to be implied by the prior decisions (i.e. they
// can be enqueued at this level).
// TODO(user): It'd be good to store an earlier level at which
// implications may be propagated.
class ProtoTrail {
 public:
  // Adds a new assigned level to the trail.
  void PushLevel(const ProtoLiteral& decision, IntegerValue objective_lb,
                 int node_id);

  // Asserts that the decision at `level` is implied by earlier decisions.
  void SetLevelImplied(int level);

  // Clear the trail, removing all levels.
  void Clear();

  // Set a lower bound on the objective at level.
  void SetObjectiveLb(int level, IntegerValue objective_lb);

  // Returns the maximum decision level stored in the trail.
  int MaxLevel() const { return decision_indexes_.size(); }

  // Returns the decision assigned at `level`.
  ProtoLiteral Decision(int level) const {
    CHECK_GE(level, 1);
    CHECK_LE(level, decision_indexes_.size());
    return literals_[decision_indexes_[level - 1]];
  }

  // Returns the node ids for decisions and implications at `level`.
  absl::Span<const int> NodeIds(int level) const;

  // Returns literals which may be propagated at `level`, this does not include
  // the decision.
  absl::Span<const ProtoLiteral> Implications(int level) const;

  IntegerValue ObjectiveLb(int level) const {
    CHECK_GE(level, 1);
    return level_to_objective_lbs_[level - 1];
  }

  absl::Span<const ProtoLiteral> Literals() const { return literals_; }

 private:
  // Parallel vectors encoding the literals and node ids on the trail.
  std::vector<ProtoLiteral> literals_;
  std::vector<int> node_ids_;

  // The index in the literals_/node_ids_ vectors for the start of each level.
  std::vector<int> decision_indexes_;

  // The objective lower bound of each level.
  std::vector<IntegerValue> level_to_objective_lbs_;
};

// Experimental thread-safe class for managing work assignments between workers.
// This API is intended to investigate Graeme Gange & Peter Stuckey's proposal
// "Scalable Parallelization of Learning Solvers".
// The core idea of this implementation is that workers can maintain several
// ProtoTrails, and periodically replace the "worst" one.
// With 1 assignment per worker, this leads to something very similar to
// Embarassingly Parallel Search.
class SharedTreeManager {
 public:
  explicit SharedTreeManager(Model* model);
  SharedTreeManager(const SharedTreeManager&) = delete;

  int NumWorkers() const { return num_workers_; }
  int NumNodes() const ABSL_LOCKS_EXCLUDED(mu_);

  // Returns the number of splits each worker should propose this restart.
  int SplitsToGeneratePerWorker() const;

  // Syncs the state of path with the shared search tree.
  // Clears `path` and returns false if the assigned subtree is closed or a
  // restart has invalidated the path.
  bool SyncTree(ProtoTrail& path) ABSL_LOCKS_EXCLUDED(mu_);

  // Assigns a path prefix that the worker should explore.
  void ReplaceTree(ProtoTrail& path);

  // Asserts that the subtree in path up to `level` contains no improving
  // solutions. Clears path.
  void CloseTree(ProtoTrail& path, int level);

  // Called by workers in order to split the shared tree.
  // `path` may or may not be extended by one level, branching on `decision`.
  void ProposeSplit(ProtoTrail& path, ProtoLiteral decision);

  void Restart() {
    absl::MutexLock l(&mu_);
    RestartLockHeld();
  }

 private:
  struct Node {
    ProtoLiteral literal;
    IntegerValue objective_lb = kMinIntegerValue;
    Node* parent = nullptr;
    std::array<Node*, 2> children = {nullptr, nullptr};
    // A node's id is related to its index in `nodes_`, see `node_id_offset_`.
    int id;
    bool closed = false;
    bool implied = false;
  };
  bool IsValid(const ProtoTrail& path) const ABSL_EXCLUSIVE_LOCKS_REQUIRED(mu_);
  Node* GetSibling(Node* node) ABSL_EXCLUSIVE_LOCKS_REQUIRED(mu_);
  void Split(std::vector<std::pair<Node*, int>>& nodes, ProtoLiteral lit)
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(mu_);
  Node* MakeSubtree(Node* parent, ProtoLiteral literal)
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(mu_);
  void ProcessNodeChanges() ABSL_EXCLUSIVE_LOCKS_REQUIRED(mu_);
  std::vector<std::pair<Node*, int>> GetAssignedNodes(const ProtoTrail& path)
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(mu_);
  void AssignLeaf(ProtoTrail& path, Node* leaf)
      ABSL_EXCLUSIVE_LOCKS_REQUIRED(mu_);
  void RestartLockHeld() ABSL_EXCLUSIVE_LOCKS_REQUIRED(mu_);
  std::string ShortStatus() const ABSL_EXCLUSIVE_LOCKS_REQUIRED(mu_);

  mutable absl::Mutex mu_;
  const SatParameters& params_;
  const int num_workers_;
  SharedResponseManager* const shared_response_manager_;

  // Stores the node id of the root, this is used to handle global restarts.
  int node_id_offset_ ABSL_GUARDED_BY(mu_) = 0;

  // Stores the nodes in the search tree.
  std::deque<Node> nodes_ ABSL_GUARDED_BY(mu_);
  std::vector<Node*> unassigned_leaves_ ABSL_GUARDED_BY(mu_);

  // How many splits we should generate now to keep the desired number of
  // leaves.
  int num_splits_wanted_;

  // We limit the total nodes generated per restart to cap the RAM usage and
  // communication overhead. If we exceed this, we will restart the shared tree.
  const int max_nodes_;
  int num_leaves_assigned_ ABSL_GUARDED_BY(mu_) = 0;

  // Temporary vectors used to maintain the state of the tree when nodes are
  // closed and/or children are updated.
  std::vector<Node*> to_close_ ABSL_GUARDED_BY(mu_);
  std::vector<Node*> to_update_ ABSL_GUARDED_BY(mu_);

  int64_t num_restarts_ ABSL_GUARDED_BY(mu_) = 0;
  int64_t num_syncs_since_restart_ ABSL_GUARDED_BY(mu_) = 0;
};

class SharedTreeWorker {
 public:
  explicit SharedTreeWorker(Model* model);
  SharedTreeWorker(const SharedTreeWorker&) = delete;
  SharedTreeWorker& operator=(const SharedTreeWorker&) = delete;

  SatSolver::Status Search(
      const std::function<void()>& feasible_solution_observer);

 private:
  // Syncs the assigned tree with the local trail, ensuring that any new
  // implications are synced. This is a noop if the search is deeper than the
  // assigned tree. Returns false if the problem is unsat.
  bool SyncWithLocalTrail();
  void SyncWithSharedTree();
  Literal DecodeDecision(ProtoLiteral literal);
  std::optional<ProtoLiteral> EncodeDecision(Literal decision);
  bool NextDecision(LiteralIndex* decision_index);
  void MaybeProposeSplit();

  // Add any implications to the clause database for the current level.
  // Return true if any new information was added.
  bool AddImplications(absl::Span<const ProtoLiteral> implied_literals);
  bool AddDecisionImplication(Literal literal, int level);

  const std::vector<Literal>& DecisionReason(int level);

  SatParameters* parameters_;
  SharedResponseManager* shared_response_;
  TimeLimit* time_limit_;
  SharedTreeManager* manager_;
  CpModelMapping* mapping_;
  SatSolver* sat_solver_;
  Trail* trail_;
  IntegerTrail* integer_trail_;
  IntegerEncoder* encoder_;
  const ObjectiveDefinition* objective_;
  ModelRandomGenerator* random_;
  IntegerSearchHelper* helper_;
  SearchHeuristics* heuristics_;

  int64_t num_restarts_ = 0;

  ProtoTrail assigned_tree_;
  std::vector<Literal> assigned_tree_literals_;
  // How many restarts had happened when the current tree was assigned?
  int64_t tree_assignment_restart_ = -1;

  int splits_wanted_ = 1;

  std::vector<Literal> reason_;
};

}  // namespace operations_research::sat

#endif  // OR_TOOLS_SAT_WORK_ASSIGNMENT_H_
