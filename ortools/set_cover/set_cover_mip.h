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

#ifndef OR_TOOLS_SET_COVER_SET_COVER_MIP_H_
#define OR_TOOLS_SET_COVER_SET_COVER_MIP_H_

#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "ortools/set_cover/base_types.h"
#include "ortools/set_cover/set_cover_heuristics.h"
#include "ortools/set_cover/set_cover_invariant.h"

namespace operations_research {
enum class SetCoverMipSolver : int {
  SCIP = 0,
  SAT = 1,
  GUROBI = 2,
  GLOP = 3,
  PDLP = 4
};

class SetCoverMip : public SubsetListBasedSolutionGenerator {
 public:
  // Simpler constructor that uses SCIP by default.
  explicit SetCoverMip(SetCoverInvariant* inv,
                       const absl::string_view name = "Mip")
      : SubsetListBasedSolutionGenerator(inv, name),
        mip_solver_(SetCoverMipSolver::SCIP),
        use_integers_(true) {}

  SetCoverMip& UseMipSolver(SetCoverMipSolver mip_solver) {
    mip_solver_ = mip_solver;
    return *this;
  }

  SetCoverMip& UseIntegers(bool use_integers) {
    use_integers_ = use_integers;
    return *this;
  }

  using SubsetListBasedSolutionGenerator::NextSolution;

  // Computes the next partial solution considering only the subsets whose
  // indices are in focus.
  bool NextSolution(absl::Span<const SubsetIndex> focus) final;

 private:
  // The MIP solver flavor used by the instance.
  SetCoverMipSolver mip_solver_;

  // Whether to use integer variables in the MIP.
  bool use_integers_;
};
}  // namespace operations_research

#endif  // OR_TOOLS_SET_COVER_SET_COVER_MIP_H_
