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

//
// Maximal clique algorithms, based on the Bron-Kerbosch algorithm.
// See http://en.wikipedia.org/wiki/Bron-Kerbosch_algorithm
// and
// C. Bron and J. Kerbosch, Joep, "Algorithm 457: finding all cliques of an
// undirected graph", CACM 16 (9): 575–577, 1973.
// http://dl.acm.org/citation.cfm?id=362367&bnc=1.
//
// Keywords: undirected graph, clique, clique cover, Bron, Kerbosch.

#ifndef OR_TOOLS_GRAPH_CLIQUES_H_
#define OR_TOOLS_GRAPH_CLIQUES_H_

#include <functional>
#include <unordered_set>
#include <numeric>
#include <vector>

#include "ortools/base/logging.h"
#include "ortools/base/join.h"
#include "ortools/base/int_type.h"
#include "ortools/base/int_type_indexed_vector.h"
#include "ortools/util/time_limit.h"

namespace operations_research {

// Finds all maximal cliques, even of size 1, in the
// graph described by the graph callback. graph->Run(i, j) indicates
// if there is an arc between i and j.
// This function takes ownership of 'callback' and deletes it after it has run.
// If 'callback' returns true, then the search for cliques stops.
void FindCliques(std::function<bool(int, int)> graph, int node_count,
                 std::function<bool(const std::vector<int>&)> callback);

// Covers the maximum number of arcs of the graph with cliques. The graph
// is described by the graph callback. graph->Run(i, j) indicates if
// there is an arc between i and j.
// This function takes ownership of 'callback' and deletes it after it has run.
// It calls 'callback' upon each clique.
// It ignores cliques of size 1.
void CoverArcsByCliques(std::function<bool(int, int)> graph, int node_count,
                        std::function<bool(const std::vector<int>&)> callback);

// Possible return values of the callback for reporting cliques. The returned
// value determines whether the algorithm will continue the search.
enum class CliqueResponse {
  // The algorithm will continue searching for other maximal cliques.
  CONTINUE,
  // The algorithm will stop the search immediately. The search can be resumed
  // by calling BronKerboschAlgorithm::Run (resp. RunIterations) again.
  STOP
};

// The status value returned by BronKerboschAlgorithm::Run and
// BronKerboschAlgorithm::RunIterations.
enum class BronKerboschAlgorithmStatus {
  // The algorithm has enumerated all maximal cliques.
  COMPLETED,
  // The search algorithm was interrupted either because it reached the
  // iteration limit or because the clique callback returned
  // CliqueResponse::STOP.
  INTERRUPTED
};

// Implements the Bron-Kerbosch algorithm for finding maximal cliques.
// The graph is represented as a callback that gets two nodes as its arguments
// and it returns true if and only if there is an arc between the two nodes. The
// cliques are reported back to the user using a second callback.
//
// Typical usage:
// auto graph = [](int node1, int node2) { return true; };
// auto on_clique = [](const std::vector<int>& clique) { LOG(INFO) << "Clique!"; };
//
// BronKerboschAlgorithm<int> bron_kerbosch(graph, num_nodes, on_clique);
// bron_kerbosch.Run();
//
// or:
//
// BronKerboschAlgorithm bron_kerbosch(graph, num_nodes, clique);
// bron_kerbosch.RunIterations(kMaxNumIterations);
//
// This is a non-recursive implementation of the Bron-Kerbosch algorithm with
// pivots as described in the paper by Bron and Kerbosch (1973) (the version 2
// algorithm in the paper).
// The basic idea of the algorithm is to incrementally build the cliques using
// depth-first search. During the search, the algorithm maintains two sets of
// candidates (nodes that are connected to all nodes in the current clique):
// - the "not" set - these are candidates that were already visited by the
//   search and all the maximal cliques that contain them as a part of the
//   current clique were already reported.
// - the actual candidates - these are candidates that were not visited yet, and
//   they can be added to the clique.
// In each iteration, the algorithm does the first of the following actions that
// applies:
// A. If there are no actual candidates and there are candidates in the "not"
//    set, or if all actual candidates are connected to the same node in the
//    "not" set, the current clique can't be extended to a maximal clique that
//    was not already reported. Return from the recursive call and move the
//    selected candidate to the set "not".
// B. If there are no candidates at all, it means that the current clique can't
//    be extended and that it is in fact a maximal clique. Report it to the user
//    and return from the recursive call. Move the selected candidate to the set
//    "not".
// C. Otherwise, there are actual candidates, extend the current clique with one
//    of these candidates and process it recursively.
//
// To avoid unnecessary steps, the algorithm selects a pivot at each level of
// the recursion to guide the selection of candidates added to the current
// clique. The pivot can be either in the "not" set and among the actual
// candidates. The algorithm tries to move the pivot and all actual candidates
// connected to it to the set "not" as quickly as possible. This will fulfill
// the conditions of step A, and the search algorithm will be able to leave the
// current branch. Selecting a pivot that has the lowest number of disconnected
// nodes among the candidates can reduce the running time significantly.
//
// The worst-case maximal depth of the recursion is equal to the number of nodes
// in the graph, which makes the natural recursive implementation impractical
// for nodes with more than a few thousands of nodes. To avoid the limitation,
// this class simulates the recursion by maintaining a stack with the state at
// each level of the recursion. The algorithm then runs in a loop. In each
// iteration, the algorithm can do one or both of:
// 1. Return to the previous recursion level (step A or B of the algorithm) by
//    removing the top state from the stack.
// 2. Select the next candidate and enter the next recursion level (step C of
//    the algorithm) by adding a new state to the stack.
//
// The worst-case time complexity of the algorithm is O(3^(N/3)), and the memory
// complexity is O(N^2), where N is the number of nodes in the graph.
template <typename NodeIndex>
class BronKerboschAlgorithm {
 public:
  // A callback called by the algorithm to test if there is an arc between a
  // pair of nodes. The callback must return true if and only if there is an
  // arc. Note that to function properly, the function must be symmetrical
  // (represent an undirected graph).
  using IsArcCallback = std::function<bool(NodeIndex, NodeIndex)>;
  // A callback called by the algorithm to report a maximal clique to the user.
  // The clique is returned as a list of nodes in the clique, in no particular
  // order. The caller must make a copy of the vector if they want to keep the
  // nodes.
  //
  // The return value of the callback controls how the algorithm continues after
  // this clique. See the description of the values of 'CliqueResponse' for more
  // details.
  using CliqueCallback =
      std::function<CliqueResponse(const std::vector<NodeIndex>&)>;

  // Initializes the Bron-Kerbosch algorithm for the given graph and clique
  // callback function.
  BronKerboschAlgorithm(IsArcCallback is_arc, NodeIndex num_nodes,
                        CliqueCallback clique_callback)
      : is_arc_(std::move(is_arc)),
        clique_callback_(std::move(clique_callback)),
        num_nodes_(num_nodes) {}

  // Runs the Bron-Kerbosch algorithm for kint64max iterations. In practice,
  // this is equivalent to running until completion or until the clique callback
  // returns BronKerboschAlgorithmStatus::STOP. If the method returned because
  // the search is finished, it will return COMPLETED; otherwise, it will return
  // INTERRUPTED and it can be resumed by calling this method again.
  BronKerboschAlgorithmStatus Run();

  // Runs at most 'max_num_iterations' iterations of the Bron-Kerbosch
  // algorithm. When this function returns INTERRUPTED, there is still work to
  // be done to process all the cliques in the graph. In such case the method
  // can be called again and it will resume the work where the previous call had
  // stopped. When it returns COMPLETED any subsequent call to the method will
  // resume the search from the beginning.
  BronKerboschAlgorithmStatus RunIterations(int64 max_num_iterations);

  // Runs at most 'max_num_iterations' iterations of the Bron-Kerbosch
  // algorithm, until the time limit is exceeded or until all cliques are
  // enumerated. When this function returns INTERRUPTED, there is still work to
  // be done to process all the cliques in the graph. In such case the method
  // can be called again and it will resume the work where the previous call had
  // stopped. When it returns COMPLETED any subsequent call to the method will
  // resume the search from the beginning.
  BronKerboschAlgorithmStatus RunWithTimeLimit(int64 max_num_iterations,
                                               TimeLimit* time_limit);

  // Runs the Bron-Kerbosch algorithm for at most kint64max iterations, until
  // the time limit is excceded or until all cliques are enumerated. In
  // practice, running the algorithm for kint64max iterations is equivalent to
  // running until completion or until the other stopping conditions apply. When
  // this function returns INTERRUPTED, there is still work to be done to
  // process all the cliques in the graph. In such case the method can be called
  // again and it will resume the work where the previous call had stopped. When
  // it returns COMPLETED any subsequent call to the method will resume the
  // search from the beginning.
  BronKerboschAlgorithmStatus RunWithTimeLimit(TimeLimit* time_limit) {
    return RunWithTimeLimit(kint64max, time_limit);
  }

 private:
  DEFINE_INT_TYPE(CandidateIndex, ptrdiff_t);

  // A data structure that maintains the variables of one "iteration" of the
  // search algorithm. These are the variables that would normally be allocated
  // on the stack in the recursive implementation.
  //
  // Note that most of the variables in the structure are explicitly left
  // uninitialized by the constructor to avoid wasting resources on values that
  // will be overwritten anyway. Most of the initialization is done in
  // BronKerboschAlgorithm::InitializeState.
  struct State {
    State() {}
    State(const State& other)
        : pivot(other.pivot),
          num_remaining_candidates(other.num_remaining_candidates),
          candidates(other.candidates),
          first_candidate_index(other.first_candidate_index),
          candidate_for_recursion(other.candidate_for_recursion) {}

    State& operator=(const State& other) {
      pivot = other.pivot;
      num_remaining_candidates = other.num_remaining_candidates;
      candidates = other.candidates;
      first_candidate_index = other.first_candidate_index;
      candidate_for_recursion = other.candidate_for_recursion;
      return *this;
    }

    // Moves the first candidate in the state to the "not" set. Assumes that the
    // first candidate is also the pivot or a candidate disconnected from the
    // pivot (as done by RunIteration).
    inline void MoveFirstCandidateToNotSet() {
      ++first_candidate_index;
      --num_remaining_candidates;
    }

    // Creates a human-readable representation of the current state.
    std::string DebugString() {
      std::string buffer;
      StrAppend(&buffer, "pivot = ", pivot,
                      "\nnum_remaining_candidates = ", num_remaining_candidates,
                      "\ncandidates = [");
      for (CandidateIndex i(0); i < candidates.size(); ++i) {
        if (i > 0) buffer += ", ";
        StrAppend(&buffer, candidates[i]);
      }
      StrAppend(
          &buffer, "]\nfirst_candidate_index = ", first_candidate_index.value(),
          "\ncandidate_for_recursion = ", candidate_for_recursion.value());
      return buffer;
    }

    // The pivot node selected for the given level of the recursion.
    NodeIndex pivot;
    // The number of remaining candidates to be explored at the given level of
    // the recursion; the number is computed as num_disconnected_nodes +
    // pre_increment in the original algorithm.
    int num_remaining_candidates;
    // The list of nodes that are candidates for extending the current clique.
    // This vector has the format proposed in the paper by Bron-Kerbosch; the
    // first 'first_candidate_index' elements of the vector represent the
    // "not" set of nodes that were already visited by the algorithm. The
    // remaining elements are the actual candidates for extending the current
    // clique.
    // NOTE(user): We could store the delta between the iterations; however,
    // we need to evaluate the impact this would have on the performance.
    ITIVector<CandidateIndex, NodeIndex> candidates;
    // The index of the first actual candidate in 'candidates'. This number is
    // also the number of elements of the "not" set stored at the beginning of
    // 'candidates'.
    CandidateIndex first_candidate_index;

    // The current position in candidates when looking for the pivot and/or the
    // next candidate disconnected from the pivot.
    CandidateIndex candidate_for_recursion;
  };

  // The deterministic time coefficients for the push and pop operations of the
  // Bron-Kerbosch algorithm. The coefficients are set to match approximately
  // the running time in seconds on a recent workstation on the random graph
  // benchmark.
  // NOTE(user): PushState is not the only source of complexity in the
  // algorithm, but non-negative linear least squares produced zero coefficients
  // for all other deterministic counters tested during the benchmarking. When
  // we optimize the algorithm, we might need to add deterministic time to the
  // other places that may produce complexity, namely InitializeState, PopState
  // and SelectCandidateIndexForRecursion.
  static const double kPushStateDeterministicTimeSecondsPerCandidate;

  // Initializes the root state of the algorithm.
  void Initialize();

  // Removes the top state from the state stack. This is equivalent to returning
  // in the recursive implementation of the algorithm.
  void PopState();

  // Adds a new state to the top of the stack, adding the node 'selected' to the
  // current clique. This is equivalent to making a recurisve call in the
  // recursive implementation of the algorithm.
  void PushState(NodeIndex selected);

  // Initializes the given state. Runs the pivot selection algorithm in the
  // state.
  void InitializeState(State* state);

  // Returns true if (node1, node2) is an arc in the graph or if node1 == node2.
  inline bool IsArc(NodeIndex node1, NodeIndex node2) const {
    return node1 == node2 || is_arc_(node1, node2);
  }

  // Selects the next node for recursion. The selected node is either the pivot
  // (if it is not in the set "not") or a node that is disconnected from the
  // pivot.
  CandidateIndex SelectCandidateIndexForRecursion(State* state);

  // Returns a human-readable std::string representation of the clique.
  std::string CliqueDebugString(const std::vector<NodeIndex>& clique);

  // The callback called when the algorithm needs to determine if (node1, node2)
  // is an arc in the graph.
  IsArcCallback is_arc_;

  // The callback called when the algorithm discovers a maximal clique. The
  // return value of the callback controls how the algorithm proceeds with the
  // clique search.
  CliqueCallback clique_callback_;

  // The number of nodes in the graph.
  const NodeIndex num_nodes_;

  // Contains the state of the aglorithm. The vector serves as an external stack
  // for the recursive part of the algorithm - instead of using the C++ stack
  // and natural recursion, it is implemented as a loop and new states are added
  // to the top of the stack. The algorithm ends when the stack is empty.
  std::vector<State> states_;

  // A vector that receives the current clique found by the algorithm.
  std::vector<NodeIndex> current_clique_;

  // Set to true if the algorithm is active (it was not stopped by an the clique
  // callback).
  int64 num_remaining_iterations_;

  // The current time limit used by the solver. The time limit is assigned by
  // the Run methods and it can be different for each call to run.
  TimeLimit* time_limit_;
};

template <typename NodeIndex>
void BronKerboschAlgorithm<NodeIndex>::InitializeState(State* state) {
  DCHECK(state != nullptr);
  const int num_candidates = state->candidates.size();
  int num_disconnected_candidates = num_candidates;
  state->pivot = 0;
  CandidateIndex pivot_index(-1);
  for (CandidateIndex pivot_candidate_index(0);
       pivot_candidate_index < num_candidates &&
       num_disconnected_candidates > 0;
       ++pivot_candidate_index) {
    const NodeIndex pivot_candidate = state->candidates[pivot_candidate_index];
    int count = 0;
    for (CandidateIndex i(state->first_candidate_index); i < num_candidates;
         ++i) {
      if (!IsArc(pivot_candidate, state->candidates[i])) {
        ++count;
      }
    }
    if (count < num_disconnected_candidates) {
      pivot_index = pivot_candidate_index;
      state->pivot = pivot_candidate;
      num_disconnected_candidates = count;
    }
  }
  state->num_remaining_candidates = num_disconnected_candidates;
  if (pivot_index >= state->first_candidate_index) {
    std::swap(state->candidates[pivot_index],
              state->candidates[state->first_candidate_index]);
    ++state->num_remaining_candidates;
  }
}

template <typename NodeIndex>
typename BronKerboschAlgorithm<NodeIndex>::CandidateIndex BronKerboschAlgorithm<
    NodeIndex>::SelectCandidateIndexForRecursion(State* state) {
  DCHECK(state != nullptr);
  CandidateIndex disconnected_node_index =
      std::max(state->first_candidate_index, state->candidate_for_recursion);
  while (disconnected_node_index < state->candidates.size() &&
         state->candidates[disconnected_node_index] != state->pivot &&
         IsArc(state->pivot, state->candidates[disconnected_node_index])) {
    ++disconnected_node_index;
  }
  state->candidate_for_recursion = disconnected_node_index;
  return disconnected_node_index;
}

template <typename NodeIndex>
void BronKerboschAlgorithm<NodeIndex>::Initialize() {
  DCHECK(states_.empty());
  states_.reserve(num_nodes_);
  states_.emplace_back();

  State* const root_state = &states_.back();
  root_state->first_candidate_index = 0;
  root_state->candidate_for_recursion = 0;
  root_state->candidates.resize(num_nodes_, 0);
  std::iota(root_state->candidates.begin(), root_state->candidates.end(), 0);
  root_state->num_remaining_candidates = num_nodes_;
  InitializeState(root_state);

  DVLOG(2) << "Initialized";
}

template <typename NodeIndex>
void BronKerboschAlgorithm<NodeIndex>::PopState() {
  DCHECK(!states_.empty());
  states_.pop_back();
  if (!states_.empty()) {
    State* const state = &states_.back();
    current_clique_.pop_back();
    state->MoveFirstCandidateToNotSet();
  }
}

template <typename NodeIndex>
std::string BronKerboschAlgorithm<NodeIndex>::CliqueDebugString(
    const std::vector<NodeIndex>& clique) {
  std::string message = "Clique: [ ";
  for (const NodeIndex node : clique) {
    StrAppend(&message, node, " ");
  }
  message += "]";
  return message;
}

template <typename NodeIndex>
void BronKerboschAlgorithm<NodeIndex>::PushState(NodeIndex selected) {
  DCHECK(!states_.empty());
  DCHECK(time_limit_ != nullptr);
  DVLOG(2) << "PushState: New depth = " << states_.size() + 1
           << ", selected node = " << selected;
  ITIVector<CandidateIndex, NodeIndex> new_candidates;

  State* const previous_state = &states_.back();
  const double deterministic_time =
      kPushStateDeterministicTimeSecondsPerCandidate *
      previous_state->candidates.size();
  time_limit_->AdvanceDeterministicTime(deterministic_time, "PushState");

  // Add all candidates from previous_state->candidates that are connected to
  // 'selected' in the graph to the vector 'new_candidates', skipping the node
  // 'selected'; this node is always at the position
  // 'previous_state->first_candidate_index', so we can skip it by skipping the
  // element at this particular index.
  new_candidates.reserve(previous_state->candidates.size());
  for (CandidateIndex i(0); i < previous_state->first_candidate_index; ++i) {
    const NodeIndex candidate = previous_state->candidates[i];
    if (IsArc(selected, candidate)) {
      new_candidates.push_back(candidate);
    }
  }
  const CandidateIndex new_first_candidate_index(new_candidates.size());
  for (CandidateIndex i = previous_state->first_candidate_index + 1;
       i < previous_state->candidates.size(); ++i) {
    const NodeIndex candidate = previous_state->candidates[i];
    if (IsArc(selected, candidate)) {
      new_candidates.push_back(candidate);
    }
  }

  current_clique_.push_back(selected);
  if (new_candidates.empty()) {
    // We've found a clique. Report it to the user, but do not push the state
    // because it would be popped immediately anyway.
    DVLOG(2) << CliqueDebugString(current_clique_);
    const CliqueResponse response = clique_callback_(current_clique_);
    if (response == CliqueResponse::STOP) {
      // The number of remaining iterations will be decremented at the end of
      // the loop in RunIterations; setting it to 0 here would make it -1 at
      // the end of the main loop.
      num_remaining_iterations_ = 1;
    }
    current_clique_.pop_back();
    previous_state->MoveFirstCandidateToNotSet();
    return;
  }

  // NOTE(user): The following line may invalidate previous_state (if the
  // vector data was re-allocated in the process). We must avoid using
  // previous_state below here.
  states_.emplace_back();
  State* const new_state = &states_.back();
  new_state->candidates.swap(new_candidates);
  new_state->first_candidate_index = new_first_candidate_index;

  InitializeState(new_state);
}

template <typename NodeIndex>
BronKerboschAlgorithmStatus BronKerboschAlgorithm<NodeIndex>::RunWithTimeLimit(
    int64 max_num_iterations, TimeLimit* time_limit) {
  CHECK(time_limit != nullptr);
  time_limit_ = time_limit;
  if (states_.empty()) {
    Initialize();
  }
  for (num_remaining_iterations_ = max_num_iterations;
       !states_.empty() && num_remaining_iterations_ > 0 &&
       !time_limit->LimitReached();
       --num_remaining_iterations_) {
    State* const state = &states_.back();
    DVLOG(2) << "Loop: " << states_.size() << " states, "
             << state->num_remaining_candidates << " candidate to explore\n"
             << state->DebugString();
    if (state->num_remaining_candidates == 0) {
      PopState();
      continue;
    }

    const CandidateIndex selected_index =
        SelectCandidateIndexForRecursion(state);
    DVLOG(2) << "selected_index = " << selected_index;
    const NodeIndex selected = state->candidates[selected_index];
    DVLOG(2) << "Selected candidate = " << selected;

    NodeIndex& f = state->candidates[state->first_candidate_index];
    NodeIndex& s = state->candidates[selected_index];
    std::swap(f, s);

    PushState(selected);
  }
  time_limit_ = nullptr;
  return states_.empty() ? BronKerboschAlgorithmStatus::COMPLETED
                         : BronKerboschAlgorithmStatus::INTERRUPTED;
}

template <typename NodeIndex>
BronKerboschAlgorithmStatus BronKerboschAlgorithm<NodeIndex>::RunIterations(
    int64 max_num_iterations) {
  TimeLimit time_limit(std::numeric_limits<double>::infinity());
  return RunWithTimeLimit(max_num_iterations, &time_limit);
}

template <typename NodeIndex>
BronKerboschAlgorithmStatus BronKerboschAlgorithm<NodeIndex>::Run() {
  return RunIterations(kint64max);
}

template <typename NodeIndex>
const double BronKerboschAlgorithm<
    NodeIndex>::kPushStateDeterministicTimeSecondsPerCandidate = 0.54663e-7;
}  // namespace operations_research

#endif  // OR_TOOLS_GRAPH_CLIQUES_H_
