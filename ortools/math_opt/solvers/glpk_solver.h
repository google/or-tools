// Copyright 2010-2022 Google LLC
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

#ifndef OR_TOOLS_MATH_OPT_SOLVERS_GLPK_SOLVER_H_
#define OR_TOOLS_MATH_OPT_SOLVERS_GLPK_SOLVER_H_

#include <cstdint>
#include <memory>
#include <optional>
#include <thread>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "ortools/base/logging.h"
#include "ortools/math_opt/callback.pb.h"
#include "ortools/math_opt/core/solve_interrupter.h"
#include "ortools/math_opt/core/solver_interface.h"
#include "ortools/math_opt/model.pb.h"
#include "ortools/math_opt/model_parameters.pb.h"
#include "ortools/math_opt/model_update.pb.h"
#include "ortools/math_opt/parameters.pb.h"
#include "ortools/math_opt/result.pb.h"

extern "C" {
#include <glpk.h>
}

namespace operations_research {
namespace math_opt {

class GlpkSolver : public SolverInterface {
 public:
  static absl::StatusOr<std::unique_ptr<SolverInterface>> New(
      const ModelProto& model, const InitArgs& init_args);

  ~GlpkSolver() override;

  absl::StatusOr<SolveResultProto> Solve(
      const SolveParametersProto& parameters,
      const ModelSolveParametersProto& model_parameters,
      MessageCallback message_cb,
      const CallbackRegistrationProto& callback_registration, Callback cb,
      SolveInterrupter* interrupter) override;
  absl::StatusOr<bool> Update(const ModelUpdateProto& model_update) override;

 private:
  // The columns of the GPLK problem.
  //
  // This structures is intentionally similar to LinearConstraints so that some
  // template function can accept either of those to share code between rows and
  // columns. For this purpose it also defines some aliases to some GLPK
  // functions and the IsInteger() (which is trivially implemented for
  // LinearConstraints).
  struct Variables {
    static constexpr auto kSetBounds = glp_set_col_bnds;
    static constexpr auto kGetLb = glp_get_col_lb;
    static constexpr auto kGetUb = glp_get_col_ub;
    static constexpr auto kGetType = glp_get_col_type;
    static constexpr auto kDelElts = glp_del_cols;

    // Returns true if the given one-based column is an integer variable.
    static inline bool IsInteger(glp_prob* const problem, const int j);

    // The MathOpt variable id of each column in GLPK. This is zero-based, the
    // first column corresponds to the 0 and the ids.size() matches the number
    // of columns.
    //
    // The id_to_index map can be used to get the GLPK column index of a given
    // MathOpt variable id but the return value will be one-based (the
    // convention used in GLPK). Thus this invariant holds:
    //
    //   for all i in [0, num_cols), id_to_index.at(ids[i]) == i + 1
    std::vector<int64_t> ids;

    // Map each MathOpt variable id to the column one-based index in GLPK (thus
    // values are in [1, num_cols]). See the ids vector for the counter part.
    absl::flat_hash_map<int64_t, int> id_to_index;

    // The unrounded lower bound value of each column.
    //
    // We keep this value since GLPK's glp_intopt() expects integer bounds for
    // integer variables. We need the unrounded value when the type of a
    // variable is changed to continuous though by an update.
    std::vector<double> unrounded_lower_bounds;

    // The unrounded upper bound value of each column.
    //
    // See unrounded_lower_bounds documentation for details..
    std::vector<double> unrounded_upper_bounds;
  };

  // The columns of the GPLK problem.
  //
  // See the comment on Variables for details.
  struct LinearConstraints {
    static constexpr auto kSetBounds = glp_set_row_bnds;
    static constexpr auto kGetLb = glp_get_row_lb;
    static constexpr auto kGetUb = glp_get_row_ub;
    static constexpr auto kGetType = glp_get_row_type;
    static constexpr auto kDelElts = glp_del_rows;

    // Returns false. This function mirrors Variables::IsInteger() and enables
    // sharing code between variables and constraints.
    static bool IsInteger(glp_prob*, int) { return false; }

    // The MathOpt linear constraint id of each row in GLPK. This is zero-based,
    // the first row corresponds to the 0 and ids.size() matches the number of
    // rows.
    //
    // The id_to_index map can be used to get the GLPK row index of a given
    // MathOpt variable id but the return value will be one-based (the
    // convention used in GLPK). Thus this invariant holds:
    //
    //   for all i in [0, num_rows), id_to_index.at(ids[i]) == i + 1
    std::vector<int64_t> ids;

    // Map each MathOpt linear constraint id to the row one-based index in GLPK
    // (thus values are in [1, num_rows]). See the ids vector for the counter
    // part.
    absl::flat_hash_map<int64_t, int> id_to_index;
  };

  explicit GlpkSolver(const ModelProto& model);

  // Appends the variables to GLPK cols.
  void AddVariables(const VariablesProto& new_variables);

  // Appends the variables to GLPK rows.
  void AddLinearConstraints(
      const LinearConstraintsProto& new_linear_constraints);

  // Updates the objective coefficients with the new values in
  // coefficients_proto.
  void UpdateObjectiveCoefficients(
      const SparseDoubleVectorProto& coefficients_proto);

  // Updates the constraints matrix with the new values in matrix_updates.
  //
  // The first_new_(var|cstr)_id are the smallest ids of the new
  // variables/constraints (in MathOpt the same id is never reused thus all
  // variables with ids greater or equal to these values are new). A nullopt
  // value means that there are not new variables/constraints.
  void UpdateLinearConstraintMatrix(
      const SparseDoubleMatrixProto& matrix_updates,
      std::optional<int64_t> first_new_var_id,
      std::optional<int64_t> first_new_cstr_id);

  // Adds the primal solution (if it exists) to the result using the provided
  // functions to get the status of the solution (GLP_FEAS, ...), its
  // objective value and the structural variables values.
  //
  // Here `col_val` is a functions that takes a column index (i.e. the index of
  // a structural variable) and returns its primal value in the solution.
  void AddPrimalSolution(int (*get_prim_stat)(glp_prob*),
                         double (*obj_val)(glp_prob*),
                         double (*col_val)(glp_prob*, int),
                         const ModelSolveParametersProto& model_parameters,
                         SolutionProto& solution_proto);

  // Adds the dual solution (if it exists) to the result. This function must
  // only be called after having solved an LP, with the provided methods
  // depending on the type of LP solved.
  //
  // Here `col_dual` is a function that takes a column index (i.e. the index of
  // a structural variable) and returns its dual value in the solution. The
  // `row_dual` does the same for a row index (i.e. the index of an auxiliary
  // variable associated to a constraint).
  void AddDualSolution(int (*get_dual_stat)(glp_prob*),
                       double (*obj_val)(glp_prob*),
                       double (*row_dual)(glp_prob*, int),
                       double (*col_dual)(glp_prob*, int),
                       const ModelSolveParametersProto& model_parameters,
                       SolutionProto& solution_proto);

  // Adds a primal or dual ray to the result depending on the value returned by
  // glp_get_unbnd_ray().
  absl::Status AddPrimalOrDualRay(
      const ModelSolveParametersProto& model_parameters,
      SolveResultProto& result);

  // Returns an error if the current thread is no thread_id_.
  absl::Status CheckCurrentThread();

  // Id of the thread where GlpkSolver was called.
  const std::thread::id thread_id_;

  glp_prob* const problem_;

  Variables variables_;
  LinearConstraints linear_constraints_;
};

////////////////////////////////////////////////////////////////////////////////
// Inline functions implementation.
////////////////////////////////////////////////////////////////////////////////

bool GlpkSolver::Variables::IsInteger(glp_prob* const problem, const int j) {
  const int kind = glp_get_col_kind(problem, j);
  switch (kind) {
    case GLP_IV:
    case GLP_BV:
      // GLP_BV is returned when the GLPK internal kind is GLP_IV and the
      // bounds are [0,1].
      return true;
    case GLP_CV:
      return false;
    default:
      LOG(FATAL) << "Unexpected column kind: " << kind;
  }
}

}  // namespace math_opt
}  // namespace operations_research

#endif  // OR_TOOLS_MATH_OPT_SOLVERS_GLPK_SOLVER_H_
