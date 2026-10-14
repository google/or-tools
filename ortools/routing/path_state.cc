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

#include "ortools/routing/path_state.h"

#include <algorithm>
#include <vector>

#include "absl/log/check.h"
#include "absl/types/span.h"

namespace operations_research::routing {

PathState::PathState(int num_nodes, std::vector<int> path_start,
                     std::vector<int> path_end)
    : num_nodes_(num_nodes),
      num_paths_(path_start.size()),
      num_nodes_threshold_(std::max(16, 4 * num_nodes_))  // Arbitrary value.
{
  DCHECK_EQ(path_start.size(), num_paths_);
  DCHECK_EQ(path_end.size(), num_paths_);
  for (int p = 0; p < num_paths_; ++p) {
    path_start_end_.push_back({path_start[p], path_end[p]});
  }
  Reset();
}

void PathState::Reset() {
  is_invalid_ = false;
  // Initial state is all unperformed: paths go from start to end directly.
  committed_index_.assign(num_nodes_, -1);
  committed_paths_.assign(num_nodes_, kUnassigned);
  committed_nodes_.assign(2 * num_paths_, -1);
  chains_.assign(num_paths_ + 1, {-1, -1});  // Reserve 1 more for sentinel.
  paths_.assign(num_paths_, {-1, -1});
  for (int path = 0; path < num_paths_; ++path) {
    const int index = 2 * path;
    const auto& [start, end] = path_start_end_[path];
    committed_index_[start] = index;
    committed_index_[end] = index + 1;

    committed_nodes_[index] = start;
    committed_nodes_[index + 1] = end;

    committed_paths_[start] = path;
    committed_paths_[end] = path;

    chains_[path] = {index, index + 2};
    paths_[path] = {path, path + 1};
  }
  chains_[num_paths_] = {0, 0};  // Sentinel.
  // Nodes that are not starts or ends are not in any path, but they still need
  // to be represented in the committed state.
  for (int node = 0; node < num_nodes_; ++node) {
    if (committed_index_[node] != -1) continue;  // node is start or end.
    committed_index_[node] = committed_nodes_.size();
    committed_nodes_.push_back(node);
  }
}

PathState::ChainRange PathState::Chains(int path) const {
  const PathBounds bounds = paths_[path];
  return PathState::ChainRange(chains_.data() + bounds.begin_index,
                               chains_.data() + bounds.end_index,
                               committed_nodes_.data());
}

PathState::NodeRange PathState::Nodes(int path) const {
  const PathBounds bounds = paths_[path];
  return PathState::NodeRange(chains_.data() + bounds.begin_index,
                              chains_.data() + bounds.end_index,
                              committed_nodes_.data());
}

void PathState::ChangePath(int path, absl::Span<const ChainBounds> chains) {
  changed_paths_.push_back(path);
  const int path_begin_index = chains_.size();
  chains_.insert(chains_.end(), chains.begin(), chains.end());
  const int path_end_index = chains_.size();
  paths_[path] = {path_begin_index, path_end_index};
  chains_.emplace_back(0, 0);  // Sentinel.
}

void PathState::ChangeLoops(absl::Span<const int> new_loops) {
  for (const int loop : new_loops) {
    // If the node was already a loop, do not add it.
    // If it was not assigned, it becomes a loop.
    if (Path(loop) == kLoop) continue;
    changed_loops_.push_back(loop);
  }
}

void PathState::Commit() {
  DCHECK(!IsInvalid());
  if (committed_nodes_.size() < num_nodes_threshold_) {
    IncrementalCommit();
  } else {
    FullCommit();
  }
}

void PathState::Revert() {
  is_invalid_ = false;
  chains_.resize(num_paths_ + 1);  // One per path + sentinel.
  for (const int path : changed_paths_) {
    paths_[path] = {path, path + 1};
  }
  changed_paths_.clear();
  changed_loops_.clear();
}

void PathState::CopyNewPathAtEndOfNodes(int path) {
  // Copy path's nodes, chain by chain.
  const PathBounds path_bounds = paths_[path];
  for (int i = path_bounds.begin_index; i < path_bounds.end_index; ++i) {
    const ChainBounds chain_bounds = chains_[i];
    committed_nodes_.insert(committed_nodes_.end(),
                            committed_nodes_.data() + chain_bounds.begin_index,
                            committed_nodes_.data() + chain_bounds.end_index);
    if (committed_paths_[committed_nodes_.back()] == path) continue;
    for (int i = chain_bounds.begin_index; i < chain_bounds.end_index; ++i) {
      const int node = committed_nodes_[i];
      committed_paths_[node] = path;
    }
  }
}

// TODO(user): Instead of copying paths at the end systematically,
// reuse some of the memory when possible.
void PathState::IncrementalCommit() {
  const int new_nodes_begin = committed_nodes_.size();
  for (const int path : ChangedPaths()) {
    const int chain_begin = committed_nodes_.size();
    CopyNewPathAtEndOfNodes(path);
    const int chain_end = committed_nodes_.size();
    chains_[path] = {chain_begin, chain_end};
  }
  // Re-index all copied nodes.
  const int new_nodes_end = committed_nodes_.size();
  for (int i = new_nodes_begin; i < new_nodes_end; ++i) {
    const int node = committed_nodes_[i];
    committed_index_[node] = i;
  }
  // New loops stay in place: only change their path to kLoop,
  // committed_index_ does not change.
  for (const int loop : ChangedLoops()) {
    committed_paths_[loop] = kLoop;
  }
  // Committed part of the state is set up, erase incremental changes.
  Revert();
}

void PathState::FullCommit() {
  // Copy all paths at the end of committed_nodes_,
  // then remove all old committed_nodes_.
  const int old_num_nodes = committed_nodes_.size();
  for (int path = 0; path < num_paths_; ++path) {
    const int new_path_begin = committed_nodes_.size() - old_num_nodes;
    CopyNewPathAtEndOfNodes(path);
    const int new_path_end = committed_nodes_.size() - old_num_nodes;
    chains_[path] = {new_path_begin, new_path_end};
  }
  committed_nodes_.erase(committed_nodes_.begin(),
                         committed_nodes_.begin() + old_num_nodes);

  // Reindex path nodes, then loop nodes.
  constexpr int kUnindexed = -1;
  committed_index_.assign(num_nodes_, kUnindexed);
  int index = 0;
  for (const int node : committed_nodes_) {
    committed_index_[node] = index++;
  }
  for (int node = 0; node < num_nodes_; ++node) {
    if (committed_index_[node] != kUnindexed) continue;
    committed_index_[node] = index++;
    committed_nodes_.push_back(node);
  }
  for (const int loop : ChangedLoops()) {
    committed_paths_[loop] = kLoop;
  }
  // Committed part of the state is set up, erase incremental changes.
  Revert();
}

}  // namespace operations_research::routing
