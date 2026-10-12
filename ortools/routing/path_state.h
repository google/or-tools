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

#ifndef ORTOOLS_ROUTING_PATH_STATE_H_
#define ORTOOLS_ROUTING_PATH_STATE_H_

#include <initializer_list>
#include <vector>

#include "absl/types/span.h"

namespace operations_research::routing {

// A PathState represents a set of paths and changes made on it.
//
// More accurately, let us define P_{num_nodes, starts, ends}-graphs the set of
// directed graphs with nodes [0, num_nodes) whose connected components are
// paths from starts[i] to ends[i] (for the same i) and loops.
// Let us fix num_nodes, starts and ends, so we call these P-graphs.
//
// A P-graph can be described by the sequence of nodes of each of its paths,
// and its set of loops. To describe a change made on a given P-graph G0 that
// yields another P-graph G1, we choose to describe G1 in terms of G0. When
// the difference between G0 and G1 is small, as is almost always the case in a
// local search setting, the description is compact, allowing for incremental
// filters to be efficient.
//
// In order to describe G1 in terms of G0 succinctly, we describe each path of
// G1 as a sequence of chains of G0. A chain of G0 is either a nonempty sequence
// of consecutive nodes of a path of G0, or a node that was a loop in G0.
// For instance, a path that was not modified from G0 to G1 has one chain,
// the sequence of all nodes in the path. Typically, local search operators
// modify one or two paths, and the resulting paths can described as sequences
// of two to four chains of G0. Paths that were modified are listed explicitly,
// allowing to iterate only on changed paths.
// The loops of G1 are described more implicitly: the loops of G1 not in G0
// are listed explicitly, but those in both G1 and G0 are not listed.
//
// A PathState object can be in two states: committed or changed.
// At construction, the object is committed, G0.
// To enter a changed state G1, one can pass modifications with ChangePath() and
// ChangeLoops(). For reasons of efficiency, a chain is described as a range of
// node indices in the representation of the committed graph G0. To that effect,
// the nodes of a path of G0 are guaranteed to have consecutive indices.
//
// Filters can then browse the change efficiently using ChangedPaths(),
// Chains(), Nodes() and ChangedLoops().
//
// Then Commit() or Revert() can be called: Commit() sets the changed state G1
// as the new committed state, Revert() erases all changes.
class PathState {
 public:
  // A Chain allows to iterate on all nodes of a chain, and access some data:
  // first node, last node, number of nodes in the chain.
  // Chain is a range, its iterator ChainNodeIterator, its value type int.
  // Chains are returned by PathChainIterator's operator*().
  class Chain;
  // A ChainRange allows to iterate on all chains of a path.
  // ChainRange is a range, its iterator Chain*, its value type Chain.
  class ChainRange;
  // A NodeRange allows to iterate on all nodes of a path.
  // NodeRange is a range, its iterator PathNodeIterator, its value type int.
  class NodeRange;

  struct ChainBounds {
    ChainBounds() = default;
    ChainBounds(int begin_index, int end_index)
        : begin_index(begin_index), end_index(end_index) {}
    int begin_index;
    int end_index;
  };
  int CommittedIndex(int node) const { return committed_index_[node]; }
  ChainBounds CommittedPathRange(int path) const { return chains_[path]; }

  // Path constructor: path_start and path_end must be disjoint,
  // their values in [0, num_nodes).
  PathState(int num_nodes, std::vector<int> path_start,
            std::vector<int> path_end);

  // Instance-constant accessors.

  // Returns the number of nodes in the underlying graph.
  int NumNodes() const { return num_nodes_; }
  // Returns the number of paths (empty paths included).
  int NumPaths() const { return num_paths_; }
  // Returns the start of a path.
  int Start(int path) const { return path_start_end_[path].start; }
  // Returns the end of a path.
  int End(int path) const { return path_start_end_[path].end; }

  // State-dependent accessors.

  static constexpr int kUnassigned = -2;
  static constexpr int kLoop = -1;
  // Returns the committed path of a given node, kLoop if it is a loop,
  // kUnassigned if it is not assigned,
  int Path(int node) const { return committed_paths_[node]; }
  // Returns the set of paths that actually changed,
  // i.e. that have more than one chain.
  const std::vector<int>& ChangedPaths() const { return changed_paths_; }
  // Returns the set of loops that were added by the change.
  const std::vector<int>& ChangedLoops() const { return changed_loops_; }
  // Returns the current range of chains of path.
  ChainRange Chains(int path) const;
  // Returns the current range of nodes of path.
  NodeRange Nodes(int path) const;

  // State modifiers.

  // Changes the path to the given sequence of chains of the committed state.
  // Chains are described by semi-open intervals. No optimization is made in
  // case two consecutive chains are actually already consecutive in the
  // committed state: they are not merged into one chain, and Chains(path) will
  // report the two chains.
  void ChangePath(int path, absl::Span<const ChainBounds> chains);
  // Same as above, but the initializer_list interface avoids the need to pass
  // a vector.
  void ChangePath(int path, const std::initializer_list<ChainBounds>& chains) {
    changed_paths_.push_back(path);
    const int path_begin_index = chains_.size();
    chains_.insert(chains_.end(), chains.begin(), chains.end());
    const int path_end_index = chains_.size();
    paths_[path] = {path_begin_index, path_end_index};
    // Always add sentinel, in case this is the last path change.
    chains_.emplace_back(0, 0);
  }

  // Describes the nodes that are newly loops in this change.
  void ChangeLoops(absl::Span<const int> new_loops);

  // Set the current state G1 as committed. See class comment for details.
  void Commit();
  // Erase incremental changes. See class comment for details.
  void Revert();
  // Sets all paths to start->end, all other nodes to kUnassigned.
  void Reset();

  // LNS Operators may not fix variables,
  // in which case we mark the candidate invalid.
  void SetInvalid() { is_invalid_ = true; }
  bool IsInvalid() const { return is_invalid_; }

 private:
  // Most structs below are named pairs of ints, for typing purposes.

  // Start and end are stored together to optimize (likely) simultaneous access.
  struct PathStartEnd {
    PathStartEnd(int start, int end) : start(start), end(end) {}
    int start;
    int end;
  };
  // Paths are ranges of chains, which are ranges of committed nodes, see below.
  struct PathBounds {
    int begin_index;
    int end_index;
  };

  // Copies nodes in chains of path at the end of nodes,
  // and sets those nodes' path member to value path.
  void CopyNewPathAtEndOfNodes(int path);
  // Commits paths in O(#{changed paths' nodes}) time,
  // increasing this object's space usage by O(|changed path nodes|).
  void IncrementalCommit();
  // Commits paths in O(num_nodes + num_paths) time,
  // reducing this object's space usage to O(num_nodes + num_paths).
  void FullCommit();

  // Instance-constant data.
  const int num_nodes_;
  const int num_paths_;
  std::vector<PathStartEnd> path_start_end_;

  // Representation of the committed and changed paths.
  // A path is a range of chains, which is a range of nodes.
  // Ranges are represented internally by indices in vectors:
  // ChainBounds are indices in committed_nodes_. PathBounds are indices in
  // chains_. When committed (after construction, Revert() or Commit()):
  // - path ranges are [path, path+1): they have one chain.
  // - chain ranges don't overlap, chains_ has an empty sentinel at the end.
  //   The sentinel allows the Nodes() iterator to maintain its current pointer
  //   to committed nodes on NodeRange::operator++().
  // - committed_nodes_ contains all nodes, both paths and loops.
  //   Actually, old duplicates will likely appear,
  //   the current version of a node is at the index given by
  //   committed_index_[node]. A Commit() can add nodes at the end of
  //   committed_nodes_ in a space/time tradeoff, but if committed_nodes_' size
  //   is above num_nodes_threshold_, Commit() must reclaim useless duplicates'
  //   space by rewriting the path/chain/nodes structure.
  // When changed (after ChangePaths() and ChangeLoops()),
  // the structure is updated accordingly:
  // - path ranges that were changed have nonoverlapping values [begin, end)
  //   where begin is >= num_paths_ + 1, i.e. new chains are stored after
  //   the committed state.
  // - additional chain ranges are stored after the committed chains and its
  //   sentinel to represent the new chains resulting from the changes.
  //   Those chains do not overlap with one another or with committed chains.
  // - committed_nodes_ are not modified, and still represent the committed
  //   paths. committed_index_ is not modified either.
  std::vector<int> committed_nodes_;
  // Maps nodes to their path in the latest committed state.
  std::vector<int> committed_paths_;
  // Maps nodes to their index in the latest committed state.
  std::vector<int> committed_index_;
  const int num_nodes_threshold_;
  std::vector<ChainBounds> chains_;
  std::vector<PathBounds> paths_;

  // Incremental information.
  std::vector<int> changed_paths_;
  std::vector<int> changed_loops_;

  // See IsInvalid() and SetInvalid().
  bool is_invalid_ = false;
};

// A Chain is a range of committed nodes.
class PathState::Chain {
 public:
  class Iterator {
   public:
    Iterator& operator++() {
      ++current_node_;
      return *this;
    }
    int operator*() const { return *current_node_; }
    bool operator!=(Iterator other) const {
      return current_node_ != other.current_node_;
    }

   private:
    // Only a Chain can construct its iterator.
    friend class PathState::Chain;
    explicit Iterator(const int* node) : current_node_(node) {}
    const int* current_node_;
  };

  // Chains hold CommittedNode* values, a Chain may be invalidated
  // if the underlying vector is modified.
  Chain(const int* begin_node, const int* end_node)
      : begin_(begin_node), end_(end_node) {}

  int NumNodes() const { return end_ - begin_; }
  int First() const { return *begin_; }
  int Last() const { return *(end_ - 1); }
  Iterator begin() const { return Iterator(begin_); }
  Iterator end() const { return Iterator(end_); }

  Chain WithoutFirstNode() const { return Chain(begin_ + 1, end_); }

 private:
  const int* const begin_;
  const int* const end_;
};

// A ChainRange is a range of Chains, committed or not.
class PathState::ChainRange {
 public:
  class Iterator {
   public:
    Iterator& operator++() {
      ++current_chain_;
      return *this;
    }
    Chain operator*() const {
      return {first_node_ + current_chain_->begin_index,
              first_node_ + current_chain_->end_index};
    }
    bool operator!=(Iterator other) const {
      return current_chain_ != other.current_chain_;
    }

   private:
    // Only a ChainRange can construct its Iterator.
    friend class ChainRange;
    Iterator(const ChainBounds* chain, const int* const first_node)
        : current_chain_(chain), first_node_(first_node) {}
    const ChainBounds* current_chain_;
    const int* const first_node_;
  };

  // ChainRanges hold ChainBounds* and CommittedNode*,
  // a ChainRange may be invalidated if on of the underlying vector is modified.
  ChainRange(const ChainBounds* const begin_chain,
             const ChainBounds* const end_chain, const int* const first_node)
      : begin_(begin_chain), end_(end_chain), first_node_(first_node) {}

  Iterator begin() const { return {begin_, first_node_}; }
  Iterator end() const { return {end_, first_node_}; }

  ChainRange DropFirstChain() const {
    return (begin_ == end_) ? *this : ChainRange(begin_ + 1, end_, first_node_);
  }

  ChainRange DropLastChain() const {
    return (begin_ == end_) ? *this : ChainRange(begin_, end_ - 1, first_node_);
  }

 private:
  const ChainBounds* const begin_;
  const ChainBounds* const end_;
  const int* const first_node_;
};

// A NodeRange allows to iterate on all nodes of a path,
// by a two-level iteration on ChainBounds* and CommittedNode* of a PathState.
class PathState::NodeRange {
 public:
  class Iterator {
   public:
    Iterator& operator++() {
      ++current_node_;
      if (current_node_ == end_node_) {
        ++current_chain_;
        // Note: dereferencing bounds is valid because there is a sentinel
        // value at the end of PathState::chains_ to that intent.
        const ChainBounds bounds = *current_chain_;
        current_node_ = first_node_ + bounds.begin_index;
        end_node_ = first_node_ + bounds.end_index;
      }
      return *this;
    }
    int operator*() const { return *current_node_; }
    bool operator!=(Iterator other) const {
      return current_chain_ != other.current_chain_;
    }

   private:
    // Only a NodeRange can construct its Iterator.
    friend class NodeRange;
    Iterator(const ChainBounds* current_chain, const int* const first_node)
        : current_node_(first_node + current_chain->begin_index),
          end_node_(first_node + current_chain->end_index),
          current_chain_(current_chain),
          first_node_(first_node) {}
    const int* current_node_;
    const int* end_node_;
    const ChainBounds* current_chain_;
    const int* const first_node_;
  };

  // NodeRanges hold ChainBounds* and int* (first committed node),
  // a NodeRange may be invalidated if on of the underlying vector is modified.
  NodeRange(const ChainBounds* begin_chain, const ChainBounds* end_chain,
            const int* first_node)
      : begin_chain_(begin_chain),
        end_chain_(end_chain),
        first_node_(first_node) {}
  Iterator begin() const { return {begin_chain_, first_node_}; }
  // Note: there is a sentinel value at the end of PathState::chains_,
  // so dereferencing chain_range_.end()->begin_ is always valid.
  Iterator end() const { return {end_chain_, first_node_}; }

 private:
  const ChainBounds* begin_chain_;
  const ChainBounds* end_chain_;
  const int* const first_node_;
};

}  // namespace operations_research::routing

#endif  // ORTOOLS_ROUTING_PATH_STATE_H_
