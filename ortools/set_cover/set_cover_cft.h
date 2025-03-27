// Copyright 2025 Francesco Cavaliere
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

#ifndef OR_TOOLS_ORTOOLS_SET_COVER_SET_COVER_CFT_H
#define OR_TOOLS_ORTOOLS_SET_COVER_SET_COVER_CFT_H

#include <absl/base/internal/pretty_function.h>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "ortools/base/strong_vector.h"
#include "ortools/set_cover/set_cover_model.h"

namespace operations_research::scp {

////////////////////////////////////////////////////////////////////////
////////////////////////// COMMON DEFINITIONS //////////////////////////
////////////////////////////////////////////////////////////////////////

// Mappings to translate btween models with different indices.
using SubsetMapVector = util_intops::StrongVector<SubsetIndex, SubsetIndex>;
using ElementMapVector = util_intops::StrongVector<ElementIndex, ElementIndex>;
using Model = SetCoverModel;

class Solution {
 public:
  double cost() const { return cost_; }
  const std::vector<SubsetIndex>& subsets() const { return subsets_; }
  std::vector<SubsetIndex>& subsets() { return subsets_; }
  void AddSubset(SubsetIndex subset, Cost cost) {
    subsets_.push_back(subset);
    cost_ += cost;
  }
  bool Empty() const { return subsets_.empty(); }

 private:
  Cost cost_;
  std::vector<SubsetIndex> subsets_;
};

class DualState {
 public:
  DualState(const Model& model);
  Cost lower_bound() const { return lower_bound_; }
  const ElementCostVector& multipliers() const { return multipliers_; }
  const SubsetCostVector& reduced_costs() const { return reduced_costs_; }

 private:
  Cost lower_bound_;
  ElementCostVector multipliers_;
  SubsetCostVector reduced_costs_;
};

struct PrimalDualState {
  Solution solution;
  DualState dual_state;
};

class CoreModel : public Model {};

absl::Status ValidateModel(const Model& model);
absl::Status ValidateFeasibleSolution(const Model& model,
                                      const Solution& solution,
                                      Cost tolerance = 1e-6);

///////////////////////////////////////////////////////////////////////
///////////////////////////// SUBGRADIENT /////////////////////////////
///////////////////////////////////////////////////////////////////////

class SubgradientCBs {};

class BoundCBs : public SubgradientCBs {};

void SubgradientOptimization(CoreModel& core_model, SubgradientCBs& cbs,
                             PrimalDualState& best_state);

///////////////////////////////////////////////////////////////////////
//////////////////////// FULL TO CORE PRICING /////////////////////////
///////////////////////////////////////////////////////////////////////

class FullToCoreModel : public CoreModel {};

////////////////////////////////////////////////////////////////////////
/////////////////////// MULTIPLIERS BASED GREEDY ///////////////////////
////////////////////////////////////////////////////////////////////////

Solution RunMultiplierBasedGreedy(
    const Model& model, const DualState& dual_state,
    Cost cost_cutoff = std::numeric_limits<BaseInt>::max());

///////////////////////////////////////////////////////////////////////
//////////////////////// THREE PHASE ALGORITHM ////////////////////////
///////////////////////////////////////////////////////////////////////

class HeuristicCBs : public SubgradientCBs {};

absl::StatusOr<PrimalDualState> RunThreePhase(
    CoreModel& model, const Solution& init_solution = {});

}  // namespace operations_research::scp

#endif /* OR_TOOLS_ORTOOLS_SET_COVER_SET_COVER_CFT_H */
