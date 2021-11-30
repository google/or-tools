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

#ifndef OR_TOOLS_MATH_OPT_SOLVERS_GUROBI_SOLVER_H_
#define OR_TOOLS_MATH_OPT_SOLVERS_GUROBI_SOLVER_H_

#include <cstdint>
#include <limits>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "ortools/base/linked_hash_map.h"
#include "ortools/math_opt/callback.pb.h"
#include "ortools/math_opt/core/solver_interface.h"
#include "ortools/math_opt/model.pb.h"
#include "ortools/math_opt/model_parameters.pb.h"
#include "ortools/math_opt/model_update.pb.h"
#include "ortools/math_opt/parameters.pb.h"
#include "ortools/math_opt/result.pb.h"
#include "ortools/math_opt/solution.pb.h"
#include "ortools/math_opt/solvers/gurobi_callback.h"
#include "ortools/math_opt/solvers/message_callback_data.h"
#include "ortools/math_opt/sparse_containers.pb.h"

#include "ortools/gurobi/environment.h"

namespace operations_research {
namespace math_opt {

class GurobiSolver : public SolverInterface {
 public:
  static absl::StatusOr<std::unique_ptr<GurobiSolver>> New(
      const ModelProto& input_model, const SolverInitializerProto& initializer);

  ~GurobiSolver() override;

  absl::StatusOr<SolveResultProto> Solve(
      const SolveParametersProto& parameters,
      const ModelSolveParametersProto& model_parameters,
      const CallbackRegistrationProto& callback_registration,
      Callback cb) override;
  absl::Status Update(const ModelUpdateProto& model_update) override;
  bool CanUpdate(const ModelUpdateProto& model_update) override;

 private:
  struct GurobiCallbackData {
    explicit GurobiCallbackData(GurobiCallbackInput callback_input)
        : callback_input(std::move(callback_input)) {}
    const GurobiCallbackInput callback_input;
    MessageCallbackData message_callback_data;
    absl::Status status = absl::OkStatus();
  };

  GurobiSolver() = default;

  // For easing reading the code, we declare these types:
  using VariableId = int64_t;
  using LinearConstraintId = int64_t;
  using GurobiVariableIndex = int;
  using GurobiLinearConstraintIndex = int;

  static constexpr GurobiVariableIndex kUnspecifiedIndex = -1;
  static constexpr GurobiLinearConstraintIndex kUnspecifiedConstraint = -2;
  static constexpr double kInf = std::numeric_limits<double>::infinity();
  // Data associated with each constraint. With it we know if the underlying
  // representation is either:
  //   linear_terms <= upper_bound (if lower bound <= -GRB_INFINITY)
  //   linear_terms >= lower_bound (if upper bound >= GRB_INFINTY)
  //   linear_terms == xxxxx_bound (if upper_bound == lower_bound)
  //   linear_term - slack == 0 (with slack bounds equal to xxxxx_bound)
  struct ConstraintData {
    GurobiLinearConstraintIndex constraint_index = kUnspecifiedConstraint;
    // only valid for true ranged constraints.
    GurobiVariableIndex slack_index = kUnspecifiedIndex;
    double lower_bound = -kInf;
    double upper_bound = kInf;
  };

  struct SlackInfo {
    LinearConstraintId id;
    ConstraintData& constraint_data;
    SlackInfo(const LinearConstraintId input_id,
              ConstraintData& input_constraint)
        : id(input_id), constraint_data(input_constraint) {}
  };

  using IdHashMap = gtl::linked_hash_map<int64_t, int>;
  using ConstraintMap = gtl::linked_hash_map<int64_t, ConstraintData>;

  // Returns a termination reason and a detailed explanation string.
  static absl::StatusOr<
      std::pair<SolveResultProto::TerminationReason, std::string>>
  ConvertTerminationReason(int gurobi_status, bool has_feasible_solution);
  absl::Status ExtractSolveResultProto(
      bool is_maximize, SolveResultProto& result,
      const ModelSolveParametersProto& model_parameters);
  absl::Status ResetParameters();
  absl::Status SetParameter(const std::string& param_name,
                            const std::string& param_value);

  // Returns a list of errors for failures only (and the empty list when all
  // parameters succeed).
  std::vector<absl::Status> SetParameters(
      const SolveParametersProto& parameters);
  absl::Status AddNewConstraints(const LinearConstraintsProto& constraints);
  absl::Status AddNewVariables(const VariablesProto& new_variables);
  absl::Status AddNewSlacks(const std::vector<SlackInfo>& new_slacks);
  absl::Status ChangeCoefficients(const SparseDoubleMatrixProto& matrix);
  absl::Status GurobiCodeToUtilStatus(int error_code, const char* source_file,
                                      int source_line,
                                      const char* statement) const;
  absl::Status LoadEnvironment();
  absl::Status LoadModel(const ModelProto& input_model);
  std::string GurobiErrorMessage(int error_code) const;
  std::string LogGurobiCode(int error_code, const char* source_file,
                            int source_line, const char* statement,
                            absl::string_view extra_message) const;
  absl::Status UpdateDoubleListAttribute(const SparseDoubleVectorProto& update,
                                         const char* attribute_name,
                                         const IdHashMap& id_hash_map);
  absl::Status UpdateGurobiIndices();
  absl::Status UpdateLinearConstraints(
      const LinearConstraintUpdatesProto& update,
      std::vector<GurobiVariableIndex>& deleted_variables_index);
  absl::StatusOr<int> GetIntAttr(const char* name) const;
  absl::StatusOr<double> GetDoubleAttr(const char* name) const;
  absl::Status GetIntAttrArray(const char* name,
                               absl::Span<int> attr_out) const;
  absl::Status GetDoubleAttrArray(const char* name,
                                  absl::Span<double> attr_out) const;
  int num_gurobi_constraints() const;
  int get_model_index(GurobiVariableIndex index) const { return index; }
  int get_model_index(const ConstraintData& index) const {
    return index.constraint_index;
  }

  // Fills in result with the values in gurobi_values aided by the index
  // conversion from map which should be either variables_map_ or
  // linear_constraints_map_ as appropriate. Only key/value pairs that passes
  // the filter predicate are added.
  template <typename T>
  void GurobiVectorToSparseDoubleVector(
      absl::Span<const double> gurobi_values, const T& map,
      SparseDoubleVectorProto& result,
      const SparseVectorFilterProto& filter) const;
  absl::StatusOr<BasisProto> GetGurobiBasis();
  absl::Status SetGurobiBasis(const BasisProto& basis);
  absl::StatusOr<DualRayProto> GetGurobiDualRay(
      const SparseVectorFilterProto& linear_constraints_filter,
      const SparseVectorFilterProto& variables_filter, bool is_maximize);
  absl::StatusOr<bool> IsLP() const;

  absl::StatusOr<std::unique_ptr<GurobiCallbackData>> RegisterCallback(
      const CallbackRegistrationProto& registration, Callback cb,
      absl::Time start);
  static int GurobiCallback(GRBmodel* model, void* cbdata, int where,
                            void* usrdata);

  // Note: Gurobi environments CAN be shared across several models, however
  // there are some caveats:
  //   - Environments are not thread-safe.
  //   - Once a gurobi_model_ is created, it makes an internal copy of the
  //     "master" environment, so, later changes to that environment will not
  //     be reflected in the gurobi_model_, for that reason we also keep
  //     around a pointer to the gurobi_model_ environment in the
  //     `active_env_` (which should not be freed).
  //   - Every "master" environment counts as a "use" of a Gurobi License.
  //     This means that if you have a limited usage count of licenses, this
  //     implementation will be consuming more licenses. On the other hand, if
  //     you have a machine license, a site license, or an academic license,
  //     this disadvantage goes away.
  //
  // TODO(user) implement a sharing master Gurobi environment mode.
  // This would be akin to the `default environment` of Gurobi in python.
  GRBenv* master_env_ = nullptr;
  GRBenv* active_env_ = nullptr;
  GRBmodel* gurobi_model_ = nullptr;
  // Note that we use linked_hash_map because the index of the gurobi_model_
  // variables/constraints is exactly the order in which they are added to the
  // model.
  // Internal correspondence from variable proto IDs to Gurobi-numbered
  // variables.
  gtl::linked_hash_map<VariableId, GurobiVariableIndex> variables_map_;
  // Internal correspondence from linear constraint proto IDs to
  // Gurobi-numbered linear constraint and extra information.
  gtl::linked_hash_map<LinearConstraintId, ConstraintData>
      linear_constraints_map_;
  // For those constraints that need a slack in the gurobi_model_
  // representation (i.e. those with
  // ConstraintData.slack_index != kUnspecifiedIndex), we keep a hash_map of
  // those LinearConstraintId's to a reference of the ConstraintData stored in
  // the linear_constraints_map_. This means that we only have one location
  // were we store information about constraints, but two places from where we
  // can access it.
  // Furthermore, the internal slack_index associated with each
  // actual slack variable is increasing in the order of them in this
  // slack_map_, and is the main reason why we choose to use
  // gtl::linked_hash_map, as the order of insertion (of the remaining
  // elements) is preserved after removals.
  // Note that after deletions, the renumbering of variables must be done
  // simultaneously for variables and slacks following the merged-order of the
  // variable_index and slack_map_.slack_index.
  gtl::linked_hash_map<LinearConstraintId, ConstraintData&> slack_map_;
  // Number of Gurobi variables, this internal quantity is updated when we
  // actually add or remove variables or constraints in the underlying Gurobi
  // model. Furthermore, since 'ModelUpdate' can trigger both creation and
  // deletion of variables and constraints, we batch those changes in two
  // steps: first add all new variables and constraints, and then delete all
  // variables and constraints that need deletion. Finally flush changes at
  // the gurobi model level (if any deletion was performed).
  int num_gurobi_variables_ = 0;

  static constexpr int kGrbBasicConstraint = 0;
  static constexpr int kGrbNonBasicConstraint = -1;
};

}  // namespace math_opt
}  // namespace operations_research

#endif  // OR_TOOLS_MATH_OPT_SOLVERS_GUROBI_SOLVER_H_
