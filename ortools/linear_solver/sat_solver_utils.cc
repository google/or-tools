// Copyright 2010-2021 Google LLC
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

#include "ortools/linear_solver/sat_solver_utils.h"

#include "absl/memory/memory.h"
#include "ortools/glop/parameters.pb.h"
#include "ortools/glop/preprocessor.h"
#include "ortools/lp_data/proto_utils.h"

namespace operations_research {

#define ADD_LP_PREPROCESSOR(name) \
  names.push_back(#name);         \
  lp_preprocessors.push_back(absl::make_unique<name>(&glop_params));

glop::ProblemStatus ApplyMipPresolveSteps(
    const glop::GlopParameters& glop_params, MPModelProto* model,
    std::vector<std::unique_ptr<glop::Preprocessor>>* for_postsolve,
    SolverLogger* logger) {
  CHECK(model != nullptr);

  // TODO(user): General constraints are currently not supported.
  if (!model->general_constraint().empty()) {
    return glop::ProblemStatus::INIT;
  }

  // We need to copy the hint because LinearProgramToMPModelProto() loose it.
  const bool hint_is_present = model->has_solution_hint();
  const auto copy_of_hint = model->solution_hint();

  // TODO(user): Remove this back and forth conversion. We could convert
  // the LinearProgram directly to a CpModelProto, or we could have a custom
  // implementation of these presolve steps.
  glop::LinearProgram lp;
  glop::MPModelProtoToLinearProgram(*model, &lp);

  // These presolve might change the problem size.
  //
  // TODO(user): transform the hint instead of disabling presolve.
  if (!hint_is_present) {
    const std::string header =
        "Running basic LP presolve, initial problem dimensions: ";
    SOLVER_LOG(logger, "");
    SOLVER_LOG(logger, header, lp.GetDimensionString());
    std::vector<std::string> names;
    std::vector<std::unique_ptr<glop::Preprocessor>> lp_preprocessors;
    ADD_LP_PREPROCESSOR(glop::FixedVariablePreprocessor);
    ADD_LP_PREPROCESSOR(glop::SingletonPreprocessor);
    ADD_LP_PREPROCESSOR(glop::ForcingAndImpliedFreeConstraintPreprocessor);
    ADD_LP_PREPROCESSOR(glop::FreeConstraintPreprocessor);

    // TODO(user): Usually it is good to run the ImpliedFreePreprocessor before
    // this one. However this seems to cause problem on atm20-100.mps. Moreover,
    // for the conversion, it is better to have tight bounds even if the bound
    // propagator is supposed to undo what this presolve would have done.
    ADD_LP_PREPROCESSOR(glop::UnconstrainedVariablePreprocessor);

    for (int i = 0; i < lp_preprocessors.size(); ++i) {
      auto& preprocessor = lp_preprocessors[i];
      preprocessor->UseInMipContext();
      const bool need_postsolve = preprocessor->Run(&lp);
      names[i].resize(header.size(), ' ');  // padding.
      SOLVER_LOG(logger, names[i], lp.GetDimensionString());
      const glop::ProblemStatus status = preprocessor->status();
      if (status != glop::ProblemStatus::INIT) return status;
      if (need_postsolve) for_postsolve->push_back(std::move(preprocessor));
    }
  }

  // Finally, we make sure all domains contain zero.
  if (!hint_is_present) {
    auto shift_bounds =
        absl::make_unique<glop::ShiftVariableBoundsPreprocessor>(&glop_params);
    shift_bounds->UseInMipContext();
    const bool need_postsolve = shift_bounds->Run(&lp);
    if (shift_bounds->status() != glop::ProblemStatus::INIT) {
      return shift_bounds->status();
    }
    if (need_postsolve) {
      for_postsolve->push_back(std::move(shift_bounds));
    }
  }

  glop::LinearProgramToMPModelProto(lp, model);

  // Restore the hint, note that none of the presolve steps we run here change
  // the number of variables in the model.
  if (hint_is_present) {
    *model->mutable_solution_hint() = copy_of_hint;
  }

  return glop::ProblemStatus::INIT;
}

#undef ADD_LP_PREPROCESSOR

}  // namespace operations_research
