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

#include "ortools/linear_solver/proto_solver/sat_solver_utils.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/log/check.h"
#include "ortools/glop/parameters.pb.h"
#include "ortools/glop/preprocessor.h"
#include "ortools/linear_solver/linear_solver.pb.h"
#include "ortools/lp_data/lp_data.h"
#include "ortools/lp_data/lp_types.h"
#include "ortools/lp_data/proto_utils.h"
#include "ortools/sat/mip_model.pb.h"
#include "ortools/sat/mp_model_wrapper.h"
#include "ortools/util/logging.h"
#include "ortools/util/time_limit.h"

namespace operations_research {

#define ADD_LP_PREPROCESSOR(name, ...) \
  names.push_back(#name);              \
  lp_preprocessors.push_back(          \
      std::make_unique<name>(&glop_params __VA_OPT__(, ) __VA_ARGS__));

namespace {

// Converts `model`, which must not have any enforced constraint, to `lp`. The
// objective is expressed in its original direction, so that converting `lp`
// back with LinearProgramToMPModelProto() and wrapping the result gives back
// the same objective scaling.
void WrappedModelToLinearProgram(const sat::MPModelProtoWrapper& model,
                                 glop::LinearProgram* lp) {
  const double scaling = model.objective_scaling();
  CHECK(scaling == 1.0 || scaling == -1.0);
  lp->Clear();
  lp->SetName(model.name());
  lp->SetMaximizationProblem(scaling < 0.0);
  lp->SetObjectiveOffset(scaling * model.objective_offset());
  for (int v = 0; v < model.num_variables(); ++v) {
    const sat::MipVariableProto& var = model.variables(v);
    const glop::ColIndex col = lp->CreateNewVariable();
    lp->SetVariableName(col, var.name());
    lp->SetVariableBounds(col, var.lower_bound(), var.upper_bound());
    lp->SetObjectiveCoefficient(col, scaling * var.objective_coefficient());
    if (var.is_integer()) {
      lp->SetVariableType(col, glop::LinearProgram::VariableType::INTEGER);
    }
  }
  for (int c = 0; c < model.num_constraints(); ++c) {
    const sat::EnforcedLinearConstraintProto& ct = model.constraints(c);
    CHECK(ct.enforcement_vars().empty());
    const glop::RowIndex row = lp->CreateNewConstraint();
    lp->SetConstraintName(row, ct.name());
    lp->SetConstraintBounds(row, ct.lower_bound(), ct.upper_bound());
    for (int i = 0; i < ct.vars().size(); ++i) {
      lp->SetCoefficient(row, glop::ColIndex(ct.vars(i)), ct.coeffs(i));
    }
  }
  lp->CleanUp();
}

}  // namespace

glop::ProblemStatus ApplyMipPresolveSteps(
    const glop::GlopParameters& glop_params, sat::MPModelProtoWrapper* model,
    std::vector<std::unique_ptr<glop::Preprocessor>>* for_postsolve,
    int* num_presolved_constraints, SolverLogger* logger) {
  CHECK(model != nullptr);
  CHECK(num_presolved_constraints != nullptr);
  *num_presolved_constraints = model->num_constraints();

  // TODO(user): Enforced constraints are currently not supported.
  for (int c = 0; c < model->num_constraints(); ++c) {
    if (!model->constraints(c).enforcement_vars().empty()) {
      return glop::ProblemStatus::INIT;
    }
  }

  // These presolve might change the problem size.
  //
  // TODO(user): transform the hint instead of disabling presolve.
  if (model->has_solution_hint()) {
    return glop::ProblemStatus::INIT;
  }

  // The integrality restrictions of the non-integer variables cannot be
  // expressed in a glop::LinearProgram, so they would be lost.
  for (int v = 0; v < model->num_variables(); ++v) {
    const sat::MipVariableProto& var = model->variables(v);
    if (!var.is_integer() && var.maximum_distance_to_integer() < 0.5) {
      return glop::ProblemStatus::INIT;
    }
  }

  std::unique_ptr<TimeLimit> time_limit =
      TimeLimit::FromParameters(glop_params);

  // TODO(user): Remove this back and forth conversion. We could convert
  // the LinearProgram directly to a CpModelProto, or we could have a custom
  // implementation of these presolve steps.
  glop::LinearProgram lp;
  WrappedModelToLinearProgram(*model, &lp);

  std::vector<std::string> names;
  std::vector<std::unique_ptr<glop::Preprocessor>> lp_preprocessors;
  const std::string header =
      "Running basic LP presolve, initial problem dimensions: ";
  SOLVER_LOG(logger, "");
  SOLVER_LOG(logger, header, lp.GetDimensionString());
  ADD_LP_PREPROCESSOR(glop::FixedVariablePreprocessor);
  ADD_LP_PREPROCESSOR(glop::SingletonPreprocessor);
  ADD_LP_PREPROCESSOR(glop::ForcingAndImpliedFreeConstraintPreprocessor);
  const int free_constraint_index = lp_preprocessors.size();
  ADD_LP_PREPROCESSOR(glop::FreeConstraintPreprocessor);

  // The SingletonPreprocessor can make a row free without removing it, in which
  // case it does not see the columns that become singleton once the row is
  // gone. This typically happens when the objective is defined through a chain
  // of equalities like obj = sum_k z_k and z_k = sum_i w_i * s_i: obj is
  // substituted but the z_k are kept. Substituting them as well moves the
  // costs onto the s_i, which is important to bound them by dual reasoning
  // instead of clamping them. So we run it a second time, but only if some
  // free rows were removed.
  const int second_singleton_index = lp_preprocessors.size();
  ADD_LP_PREPROCESSOR(glop::SingletonPreprocessor);

  // TODO(user): Usually it is good to run the ImpliedFreePreprocessor before
  // this one. However this seems to cause problem on atm20-100.mps. Moreover,
  // for the conversion, it is better to have tight bounds even if the bound
  // propagator is supposed to undo what this presolve would have done.
  ADD_LP_PREPROCESSOR(glop::UnconstrainedVariablePreprocessor);

  bool removed_free_rows = false;
  for (int i = 0; i < lp_preprocessors.size(); ++i) {
    if (time_limit->LimitReached()) break;
    if (i == second_singleton_index && !removed_free_rows) continue;
    auto& preprocessor = lp_preprocessors[i];
    preprocessor->SetTimeLimit(time_limit.get());
    preprocessor->UseInMipContext();
    const glop::Preprocessor::Result result = preprocessor->Run(&lp);
    names[i].resize(header.size(), ' ');  // padding.
    SOLVER_LOG(logger, names[i], lp.GetDimensionString());
    if (result.solve_status.has_value()) {
      return result.solve_status->problem_status();
    }
    // The FreeConstraintPreprocessor needs a postsolve iff it removed rows.
    if (i == free_constraint_index) {
      removed_free_rows = result.postsolve_is_needed;
    }
    if (result.postsolve_is_needed) {
      for_postsolve->push_back(std::move(preprocessor));
    }
  }

  MPModelProto presolved_model;
  glop::LinearProgramToMPModelProto(lp, &presolved_model);
  *num_presolved_constraints = presolved_model.constraint().size();
  *model = sat::MPModelProtoWrapper(std::move(presolved_model), logger);
  return glop::ProblemStatus::INIT;
}

#undef ADD_LP_PREPROCESSOR

}  // namespace operations_research
