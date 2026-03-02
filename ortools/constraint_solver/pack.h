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

#ifndef ORTOOLS_CONSTRAINT_SOLVER_PACK_H_
#define ORTOOLS_CONSTRAINT_SOLVER_PACK_H_

#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "ortools/constraint_solver/constraint_solver.h"

namespace operations_research {

/// This constraint packs all variables onto 'number_of_bins'
/// variables.  For any given variable, a value of 'number_of_bins'
/// indicates that the variable is not assigned to any bin.
/// Dimensions, i.e., cumulative constraints on this packing, can be
/// added directly from the pack class.
class Pack : public Constraint {
 public:
  Pack(Solver* s, const std::vector<IntVar*>& vars, int number_of_bins);

  ~Pack() override;

  /// Dimensions are additional constraints than can restrict what is
  /// possible with the pack constraint. It can be used to set capacity
  /// limits, to count objects per bin, to compute unassigned
  /// penalties...

  /// This dimension imposes that for all bins b, the weighted sum
  /// (weights[i]) of all objects i assigned to 'b' is less or equal
  /// 'bounds[b]'.
  void AddWeightedSumLessOrEqualConstantDimension(
      const std::vector<int64_t>& weights, const std::vector<int64_t>& bounds);

  /// This dimension imposes that for all bins b, the weighted sum
  /// (weights->Run(i)) of all objects i assigned to 'b' is less or
  /// equal to 'bounds[b]'. Ownership of the callback is transferred to
  /// the pack constraint.
  void AddWeightedSumLessOrEqualConstantDimension(
      Solver::IndexEvaluator1 weights, const std::vector<int64_t>& bounds);

  /// This dimension imposes that for all bins b, the weighted sum
  /// (weights->Run(i, b)) of all objects i assigned to 'b' is less or
  /// equal to 'bounds[b]'. Ownership of the callback is transferred to
  /// the pack constraint.
  void AddWeightedSumLessOrEqualConstantDimension(
      Solver::IndexEvaluator2 weights, const std::vector<int64_t>& bounds);

  /// This dimension imposes that for all bins b, the weighted sum
  /// (weights[i]) of all objects i assigned to 'b' is equal to loads[b].
  void AddWeightedSumEqualVarDimension(const std::vector<int64_t>& weights,
                                       const std::vector<IntVar*>& loads);

  /// This dimension imposes that for all bins b, the weighted sum
  /// (weights->Run(i, b)) of all objects i assigned to 'b' is equal to
  /// loads[b].
  void AddWeightedSumEqualVarDimension(Solver::IndexEvaluator2 weights,
                                       const std::vector<IntVar*>& loads);

  /// This dimension imposes:
  /// forall b in bins,
  ///    sum (i in items: usage[i] * is_assigned(i, b)) <= capacity[b]
  /// where is_assigned(i, b) is true if and only if item i is assigned
  /// to the bin b.
  ///
  /// This can be used to model shapes of items by linking variables of
  /// the same item on parallel dimensions with an allowed assignment
  /// constraint.
  void AddSumVariableWeightsLessOrEqualConstantDimension(
      const std::vector<IntVar*>& usage, const std::vector<int64_t>& capacity);

  /// This dimension enforces that cost_var == sum of weights[i] for
  /// all objects 'i' assigned to a bin.
  void AddWeightedSumOfAssignedDimension(const std::vector<int64_t>& weights,
                                         IntVar* cost_var);

  /// This dimension links 'count_var' to the actual number of bins used in the
  /// pack.
  void AddCountUsedBinDimension(IntVar* count_var);

  /// This dimension links 'count_var' to the actual number of items
  /// assigned to a bin in the pack.
  void AddCountAssignedItemsDimension(IntVar* count_var);

  void Post() override;
  void ClearAll();
  void PropagateDelayed();
  void InitialPropagate() override;
  void Propagate();
  void OneDomain(int var_index);
  std::string DebugString() const override;
  bool IsUndecided(int var_index, int bin_index) const;
  void SetImpossible(int var_index, int bin_index);
  void Assign(int var_index, int bin_index);
  bool IsAssignedStatusKnown(int var_index) const;
  bool IsPossible(int var_index, int bin_index) const;
  IntVar* AssignVar(int var_index, int bin_index) const;
  void SetAssigned(int var_index);
  void SetUnassigned(int var_index);
  void RemoveAllPossibleFromBin(int bin_index);
  void AssignAllPossibleToBin(int bin_index);
  void AssignFirstPossibleToBin(int bin_index);
  void AssignAllRemainingItems();
  void UnassignAllRemainingItems();
  void Accept(ModelVisitor* visitor) const override;

 private:
  bool IsInProcess() const;
  const std::vector<IntVar*> vars_;
  const int bins_;
  std::vector<Dimension*> dims_;
  std::unique_ptr<RevBitMatrix> unprocessed_;
  std::vector<std::vector<int>> forced_;
  std::vector<std::vector<int>> removed_;
  std::vector<IntVarIterator*> holes_;
  uint64_t stamp_;
  Demon* demon_;
  std::vector<std::pair<int, int>> to_set_;
  std::vector<std::pair<int, int>> to_unset_;
  bool in_process_;
};

}  // namespace operations_research

#endif  // ORTOOLS_CONSTRAINT_SOLVER_PACK_H_
