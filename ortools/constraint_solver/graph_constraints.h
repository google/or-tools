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

#ifndef ORTOOLS_CONSTRAINT_SOLVER_GRAPH_CONSTRAINTS_H_
#define ORTOOLS_CONSTRAINT_SOLVER_GRAPH_CONSTRAINTS_H_

#include <cstdint>
#include <deque>
#include <string>
#include <utility>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/types/span.h"
#include "ortools/constraint_solver/constraint_solver.h"
#include "ortools/util/bitset.h"

namespace operations_research {

// This constraint ensures there are no cycles in the variable/value graph.
// "Sink" values are values outside the range of the array of variables; they
// are used to end paths.
// The constraint does essentially two things:
// - forbid partial paths from looping back to themselves
// - ensure each variable/node can be connected to a "sink".
// If assume_paths is true, the constraint assumes the 'next' variables
// represent paths (and performs a faster propagation); otherwise the
// constraint assumes the 'next' variables represent a forest.
// TODO(user): improve code when assume_paths is false (currently does an
// expensive n^2 loop).

class NoCycle : public Constraint {
 public:
  NoCycle(Solver* s, const std::vector<IntVar*>& nexts,
          const std::vector<IntVar*>& active, Solver::IndexFilter1 sink_handler,
          bool assume_paths);
  ~NoCycle() override;
  void Post() override;
  void InitialPropagate() override;
  void NextChange(int index);
  void ActiveBound(int index);
  void NextBound(int index);
  void ComputeSupports();
  void ComputeSupport(int index);
  std::string DebugString() const override;
  void Accept(ModelVisitor* visitor) const override;

 private:
  int64_t size() const;

  const std::vector<IntVar*> nexts_;
  const std::vector<IntVar*> active_;
  std::vector<IntVarIterator*> iterators_;
  RevArray<int64_t> starts_;
  RevArray<int64_t> ends_;
  RevArray<bool> marked_;
  bool all_nexts_bound_;
  std::vector<int64_t> outbound_supports_;
  std::vector<int64_t> support_leaves_;
  std::vector<int64_t> unsupported_;
  Solver::IndexFilter1 sink_handler_;
  std::vector<int64_t> sinks_;
  bool assume_paths_;
};

class Circuit : public Constraint {
 public:
  Circuit(Solver* s, const std::vector<IntVar*>& nexts, bool sub_circuit);
  ~Circuit() override;
  void Post() override;
  void InitialPropagate() override;
  std::string DebugString() const override;
  void Accept(ModelVisitor* visitor) const override;

 private:
  bool Inactive(int index) const;
  void NextBound(int index);
  void NextDomain(int index);
  void TryInsertReached(int candidate, int64_t after);
  void CheckReachabilityFromRoot();
  void CheckReachabilityToRoot();

  const std::vector<IntVar*> nexts_;
  const int size_;
  std::vector<int> insertion_queue_;
  std::vector<int> to_visit_;
  std::vector<bool> reached_;
  RevArray<int> starts_;
  RevArray<int> ends_;
  RevArray<int> lengths_;
  std::vector<IntVarIterator*> domains_;
  std::vector<int> outbound_support_;
  std::vector<int> inbound_support_;
  std::vector<int> temp_support_;
  Demon* inbound_demon_;
  Demon* outbound_demon_;
  Rev<int> root_;
  NumericalRev<int> num_inactives_;
  const bool sub_circuit_;
};

class BasePathCumul : public Constraint {
 public:
  BasePathCumul(Solver* s, const std::vector<IntVar*>& nexts,
                const std::vector<IntVar*>& active,
                const std::vector<IntVar*>& cumuls);
  ~BasePathCumul() override;
  void Post() override;
  void InitialPropagate() override;
  void ActiveBound(int index);
  virtual void NextBound(int index) = 0;
  virtual bool AcceptLink(int i, int j) const = 0;
  void UpdateSupport(int index);
  void CumulRange(int index);
  std::string DebugString() const override;

 protected:
  int64_t size() const;
  int cumul_size() const;

  const std::vector<IntVar*> nexts_;
  const std::vector<IntVar*> active_;
  const std::vector<IntVar*> cumuls_;
  RevArray<int> prevs_;
  std::vector<int> supports_;
};

class PathCumul : public BasePathCumul {
 public:
  PathCumul(Solver* s, const std::vector<IntVar*>& nexts,
            const std::vector<IntVar*>& active,
            const std::vector<IntVar*>& cumuls,
            const std::vector<IntVar*>& transits);
  ~PathCumul() override;
  void Post() override;
  void NextBound(int index) override;
  bool AcceptLink(int i, int j) const override;
  void TransitRange(int index);
  void Accept(ModelVisitor* visitor) const override;

 private:
  const std::vector<IntVar*> transits_;
};

template <class T>
class StampedVector {
 public:
  StampedVector();
  const std::vector<T>& Values(Solver* solver);
  void PushBack(Solver* solver, const T& value);
  void Clear(Solver* solver);

 private:
  void CheckStamp(Solver* solver);

  std::vector<T> values_;
  uint64_t stamp_;
};

class DelayedPathCumul : public Constraint {
 public:
  DelayedPathCumul(Solver* solver, const std::vector<IntVar*>& nexts,
                   const std::vector<IntVar*>& active,
                   const std::vector<IntVar*>& cumuls,
                   const std::vector<IntVar*>& transits);
  ~DelayedPathCumul() override;
  void Post() override;
  void InitialPropagate() override;
  void NextBound(int index);
  void ActiveBound(int index);
  void PropagatePaths();
  void Accept(ModelVisitor* visitor) const override;
  std::string DebugString() const override;

 private:
  void CumulRange(int64_t index);
  void UpdateSupport(int index);
  void PropagateLink(int64_t index, int64_t next);
  bool AcceptLink(int index, int next) const;

  const std::vector<IntVar*> nexts_;
  const std::vector<IntVar*> active_;
  const std::vector<IntVar*> cumuls_;
  const std::vector<IntVar*> transits_;
  std::vector<Demon*> cumul_transit_demons_;
  Demon* path_demon_;
  StampedVector<int> touched_;
  std::vector<int64_t> chain_starts_;
  std::vector<int64_t> chain_ends_;
  std::vector<bool> is_chain_start_;
  RevArray<int> prevs_;
  std::vector<int> supports_;
  RevArray<bool> was_bound_;
  RevArray<bool> has_cumul_demon_;
};

class IndexEvaluator2PathCumul : public BasePathCumul {
 public:
  IndexEvaluator2PathCumul(Solver* s, const std::vector<IntVar*>& nexts,
                           const std::vector<IntVar*>& active,
                           const std::vector<IntVar*>& cumuls,
                           Solver::IndexEvaluator2 transit_evaluator);
  ~IndexEvaluator2PathCumul() override;
  void NextBound(int index) override;
  bool AcceptLink(int i, int j) const override;
  void Accept(ModelVisitor* visitor) const override;

 private:
  Solver::IndexEvaluator2 transits_evaluator_;
};

class IndexEvaluator2SlackPathCumul : public BasePathCumul {
 public:
  IndexEvaluator2SlackPathCumul(Solver* s, const std::vector<IntVar*>& nexts,
                                const std::vector<IntVar*>& active,
                                const std::vector<IntVar*>& cumuls,
                                const std::vector<IntVar*>& slacks,
                                Solver::IndexEvaluator2 transit_evaluator);
  ~IndexEvaluator2SlackPathCumul() override;
  void Post() override;
  void NextBound(int index) override;
  bool AcceptLink(int i, int j) const override;
  void SlackRange(int index);
  void Accept(ModelVisitor* visitor) const override;

 private:
  const std::vector<IntVar*> slacks_;
  Solver::IndexEvaluator2 transits_evaluator_;
};

class PathConnectedConstraint : public Constraint {
 public:
  PathConnectedConstraint(Solver* solver, std::vector<IntVar*> nexts,
                          absl::Span<const int64_t> sources,
                          std::vector<int64_t> sinks,
                          std::vector<IntVar*> status);
  void Post() override;
  void InitialPropagate() override;
  std::string DebugString() const override;

 private:
  void NextBound(int index);
  void EvaluatePath(int path);

  RevArray<int64_t> sources_;
  RevArray<int> index_to_path_;
  const std::vector<int64_t> sinks_;
  const std::vector<IntVar*> nexts_;
  const std::vector<IntVar*> status_;
  SparseBitset<int64_t> touched_;
};

class PathTransitPrecedenceConstraint : public Constraint {
 public:
  enum PrecedenceType {
    ANY,
    LIFO,
    FIFO,
  };
  PathTransitPrecedenceConstraint(
      Solver* solver, std::vector<IntVar*> nexts, std::vector<IntVar*> transits,
      absl::Span<const std::pair<int, int>> precedences,
      absl::flat_hash_map<int, PrecedenceType> precedence_types);
  ~PathTransitPrecedenceConstraint() override;
  void Post() override;
  void InitialPropagate() override;
  std::string DebugString() const override;
  void Accept(ModelVisitor* visitor) const override;

 private:
  void NextBound(int index);

  const std::vector<IntVar*> nexts_;
  const std::vector<IntVar*> transits_;
  std::vector<std::vector<int>> predecessors_;
  std::vector<std::vector<int>> successors_;
  const absl::flat_hash_map<int, PrecedenceType> precedence_types_;
  RevArray<int> starts_;
  RevArray<int> ends_;
  absl::flat_hash_set<int> forbidden_;
  absl::flat_hash_set<int> marked_;
  std::deque<int> pushed_;
  std::vector<int64_t> transit_cumuls_;
};

Constraint* MakePathTransitTypedPrecedenceConstraint(
    Solver* solver, std::vector<IntVar*> nexts, std::vector<IntVar*> transits,
    absl::Span<const std::pair<int, int>> precedences,
    absl::flat_hash_map<int, PathTransitPrecedenceConstraint::PrecedenceType>
        precedence_types);

class PathEnergyCostConstraint : public Constraint {
 public:
  PathEnergyCostConstraint(
      Solver* solver,
      Solver::PathEnergyCostConstraintSpecification specification);

  void Post() override;
  void InitialPropagate() override;

 private:
  void NodeDispatcher(int node);
  void PropagatePath(int path);
  Solver::PathEnergyCostConstraintSpecification specification_;
  std::vector<Demon*> path_demons_;
};

// Implementations of template methods
template <class T>
StampedVector<T>::StampedVector() : stamp_(0) {}

template <class T>
const std::vector<T>& StampedVector<T>::Values(Solver* solver) {
  CheckStamp(solver);
  return values_;
}

template <class T>
void StampedVector<T>::PushBack(Solver* solver, const T& value) {
  CheckStamp(solver);
  values_.push_back(value);
}

template <class T>
void StampedVector<T>::Clear(Solver* solver) {
  values_.clear();
  stamp_ = solver->fail_stamp();
}

template <class T>
void StampedVector<T>::CheckStamp(Solver* solver) {
  if (solver->fail_stamp() > stamp_) {
    Clear(solver);
  }
}

}  // namespace operations_research

#endif  // ORTOOLS_CONSTRAINT_SOLVER_GRAPH_CONSTRAINTS_H_
