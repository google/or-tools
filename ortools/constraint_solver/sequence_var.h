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

#ifndef ORTOOLS_CONSTRAINT_SOLVER_SEQUENCE_VAR_H_
#define ORTOOLS_CONSTRAINT_SOLVER_SEQUENCE_VAR_H_

#include <cstdint>
#include <string>
#include <vector>

#include "ortools/base/base_export.h"
#include "ortools/constraint_solver/constraint_solver.h"

namespace operations_research {

/// A sequence variable is a variable whose domain is a set of possible
/// orderings of the interval variables. It allows ordering of tasks. It
/// has two sets of methods: ComputePossibleFirstsAndLasts(), which
/// returns the list of interval variables that can be ranked first or
/// last; and RankFirst/RankNotFirst/RankLast/RankNotLast, which can be
/// used to create the search decision.
class OR_DLL SequenceVar : public PropagationBaseObject {
 public:
  SequenceVar(Solver* s, const std::vector<IntervalVar*>& intervals,
              const std::vector<IntVar*>& nexts, const std::string& name);

  ~SequenceVar() override;

  std::string DebugString() const override;

#if !defined(SWIG)
  /// Returns the minimum and maximum duration of combined interval
  /// vars in the sequence.
  void DurationRange(int64_t* dmin, int64_t* dmax) const;

  /// Returns the minimum start min and the maximum end max of all
  /// interval vars in the sequence.
  void HorizonRange(int64_t* hmin, int64_t* hmax) const;

  /// Returns the minimum start min and the maximum end max of all
  /// unranked interval vars in the sequence.
  void ActiveHorizonRange(int64_t* hmin, int64_t* hmax) const;

  /// Compute statistics on the sequence.
  void ComputeStatistics(int* ranked, int* not_ranked, int* unperformed) const;
#endif  // !defined(SWIG)

  /// Ranks the index_th interval var first of all unranked interval
  /// vars. After that, it will no longer be considered ranked.
  void RankFirst(int index);

  /// Indicates that the index_th interval var will not be ranked first
  /// of all currently unranked interval vars.
  void RankNotFirst(int index);

  /// Ranks the index_th interval var first of all unranked interval
  /// vars. After that, it will no longer be considered ranked.
  void RankLast(int index);

  /// Indicates that the index_th interval var will not be ranked first
  /// of all currently unranked interval vars.
  void RankNotLast(int index);

  /// Computes the set of indices of interval variables that can be
  /// ranked first in the set of unranked activities.
  void ComputePossibleFirstsAndLasts(std::vector<int>* possible_firsts,
                                     std::vector<int>* possible_lasts);

  /// Applies the following sequence of ranks, ranks first, then rank
  /// last.  rank_first and rank_last represents different directions.
  /// rank_first[0] corresponds to the first interval of the sequence.
  /// rank_last[0] corresponds to the last interval of the sequence.
  /// All intervals in the unperformed vector will be marked as such.
  void RankSequence(const std::vector<int>& rank_first,
                    const std::vector<int>& rank_last,
                    const std::vector<int>& unperformed);

  /// Clears 'rank_first' and 'rank_last', and fills them with the
  /// intervals in the order of the ranks. If all variables are ranked,
  /// 'rank_first' will contain all variables, and 'rank_last' will
  /// contain none.
  /// 'unperformed' will contains all such interval variables.
  /// rank_first and rank_last represents different directions.
  /// rank_first[0] corresponds to the first interval of the sequence.
  /// rank_last[0] corresponds to the last interval of the sequence.
  void FillSequence(std::vector<int>* rank_first, std::vector<int>* rank_last,
                    std::vector<int>* unperformed) const;

  /// Returns the index_th interval of the sequence.
  IntervalVar* Interval(int index) const;

  /// Returns the next of the index_th interval of the sequence.
  IntVar* Next(int index) const;

  /// Returns the number of interval vars in the sequence.
  int64_t size() const { return intervals_.size(); }

  /// Accepts the given visitor.
  virtual void Accept(ModelVisitor* visitor) const;

 private:
  int ComputeForwardFrontier();
  int ComputeBackwardFrontier();
  void UpdatePrevious() const;

  const std::vector<IntervalVar*> intervals_;
  const std::vector<IntVar*> nexts_;
  mutable std::vector<int> previous_;
};

}  // namespace operations_research

#endif  // ORTOOLS_CONSTRAINT_SOLVER_SEQUENCE_VAR_H_
