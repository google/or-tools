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

#include "ortools/math_opt/solvers/gscip_solver.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "absl/cleanup/cleanup.h"
#include "absl/container/flat_hash_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/log/check.h"
#include "absl/log/die_if_null.h"
#include "absl/log/log.h"
#include "absl/memory/memory.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"
#include "absl/strings/string_view.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "absl/types/span.h"
#include "google/protobuf/repeated_ptr_field.h"
#include "ortools/base/linked_hash_map.h"
#include "ortools/base/map_util.h"
#include "ortools/base/protoutil.h"
#include "ortools/base/status_macros.h"
#include "ortools/math_opt/callback.pb.h"
#include "ortools/math_opt/core/invalid_indicators.h"
#include "ortools/math_opt/core/inverted_bounds.h"
#include "ortools/math_opt/core/math_opt_proto_utils.h"
#include "ortools/math_opt/core/solver_interface.h"
#include "ortools/math_opt/core/sparse_submatrix.h"
#include "ortools/math_opt/core/sparse_vector_view.h"
#include "ortools/math_opt/infeasible_subsystem.pb.h"
#include "ortools/math_opt/model.pb.h"
#include "ortools/math_opt/model_parameters.pb.h"
#include "ortools/math_opt/model_update.pb.h"
#include "ortools/math_opt/parameters.pb.h"
#include "ortools/math_opt/result.pb.h"
#include "ortools/math_opt/solution.pb.h"
#include "ortools/math_opt/solvers/gscip/gscip.h"
#include "ortools/math_opt/solvers/gscip/gscip.pb.h"
#include "ortools/math_opt/solvers/gscip/gscip_event_handler.h"
#include "ortools/math_opt/solvers/gscip/gscip_parameters.h"
#include "ortools/math_opt/solvers/gscip/math_opt_gscip_solver_constraint_handler.h"
#include "ortools/math_opt/solvers/message_callback_data.h"
#include "ortools/math_opt/sparse_containers.pb.h"
#include "ortools/math_opt/validators/callback_validator.h"
#include "ortools/port/proto_utils.h"
#include "ortools/util/solve_interrupter.h"
#include "scip/type_cons.h"
#include "scip/type_var.h"

namespace operations_research {
namespace math_opt {

namespace {

constexpr double kInf = std::numeric_limits<double>::infinity();

constexpr SupportedProblemStructures kGscipSupportedStructures = {
    .integer_variables = SupportType::kSupported,
    .quadratic_objectives = SupportType::kSupported,
    .quadratic_constraints = SupportType::kSupported,
    .sos1_constraints = SupportType::kSupported,
    .sos2_constraints = SupportType::kSupported,
    .indicator_constraints = SupportType::kSupported};

int64_t SafeId(const VariablesProto& variables, int index) {
  if (variables.ids().empty()) {
    return index;
  }
  return variables.ids(index);
}

const std::string& EmptyString() {
  static const std::string* const empty_string = new std::string;
  return *empty_string;
}

const std::string& SafeName(const VariablesProto& variables, int index) {
  if (variables.names().empty()) {
    return EmptyString();
  }
  return variables.names(index);
}

int64_t SafeId(const LinearConstraintsProto& linear_constraints, int index) {
  if (linear_constraints.ids().empty()) {
    return index;
  }
  return linear_constraints.ids(index);
}

absl::string_view SafeName(const LinearConstraintsProto& linear_constraints,
                           int index) {
  if (linear_constraints.names().empty()) {
    return EmptyString();
  }
  return linear_constraints.names(index);
}

absl::flat_hash_map<int64_t, double> SparseDoubleVectorAsMap(
    const SparseDoubleVectorProto& vector) {
  CHECK_EQ(vector.ids_size(), vector.values_size());
  absl::flat_hash_map<int64_t, double> result;
  result.reserve(vector.ids_size());
  for (int i = 0; i < vector.ids_size(); ++i) {
    result[vector.ids(i)] = vector.values(i);
  }
  return result;
}

// Viewing matrix as a list of (row, column, value) tuples stored in row major
// order, does a linear scan from index scan_start to find the index of the
// first entry with row >= row_id. Returns the size the tuple list if there is
// no such entry.
inline int FindRowStart(const SparseDoubleMatrixProto& matrix,
                        const int64_t row_id, const int scan_start) {
  int result = scan_start;
  while (result < matrix.row_ids_size() && matrix.row_ids(result) < row_id) {
    ++result;
  }
  return result;
}

struct LinearConstraintView {
  int64_t linear_constraint_id;
  double lower_bound;
  double upper_bound;
  absl::string_view name;
  absl::Span<const int64_t> variable_ids;
  absl::Span<const double> coefficients;
};

// Iterates over the constraints from a LinearConstraints, outputting a
// LinearConstraintView for each constraint. Requires a SparseDoubleMatrixProto
// which may have data from additional constraints not in LinearConstraints.
//
// The running time to iterate through and read each element once is
// O(Size(*linear_constraints) + Size(*linear_constraint_matrix)).
class LinearConstraintIterator {
 public:
  LinearConstraintIterator(
      const LinearConstraintsProto* linear_constraints,
      const SparseDoubleMatrixProto* linear_constraint_matrix)
      : linear_constraints_(ABSL_DIE_IF_NULL(linear_constraints)),
        linear_constraint_matrix_(ABSL_DIE_IF_NULL(linear_constraint_matrix)) {
    if (NumConstraints(*linear_constraints_) > 0) {
      const int64_t first_constraint = SafeId(*linear_constraints_, 0);
      matrix_start_ =
          FindRowStart(*linear_constraint_matrix_, first_constraint, 0);
      matrix_end_ = FindRowStart(*linear_constraint_matrix_,
                                 first_constraint + 1, matrix_start_);
    } else {
      matrix_start_ = NumMatrixNonzeros(*linear_constraint_matrix_);
      matrix_end_ = matrix_start_;
    }
  }

  bool IsDone() const {
    return current_con_ >= NumConstraints(*linear_constraints_);
  }

  // Call only if !IsDone(). Runs in O(1).
  LinearConstraintView Current() const {
    CHECK(!IsDone());
    LinearConstraintView result;
    result.lower_bound = linear_constraints_->lower_bounds(current_con_);
    result.upper_bound = linear_constraints_->upper_bounds(current_con_);
    result.name = SafeName(*linear_constraints_, current_con_);
    result.linear_constraint_id = SafeId(*linear_constraints_, current_con_);

    const auto vars_begin = linear_constraint_matrix_->column_ids().data();
    result.variable_ids = absl::MakeConstSpan(vars_begin + matrix_start_,
                                              vars_begin + matrix_end_);
    const auto coefficients_begins =
        linear_constraint_matrix_->coefficients().data();
    result.coefficients = absl::MakeConstSpan(
        coefficients_begins + matrix_start_, coefficients_begins + matrix_end_);
    return result;
  }

  // Call only if !IsDone().
  void Next() {
    CHECK(!IsDone());
    ++current_con_;
    if (IsDone()) {
      matrix_start_ = NumMatrixNonzeros(*linear_constraint_matrix_);
      matrix_end_ = matrix_start_;
      return;
    }
    const int64_t current_row_id = SafeId(*linear_constraints_, current_con_);
    matrix_start_ =
        FindRowStart(*linear_constraint_matrix_, current_row_id, matrix_end_);

    matrix_end_ = FindRowStart(*linear_constraint_matrix_, current_row_id + 1,
                               matrix_start_);
  }

 private:
  // NOT OWNED
  const LinearConstraintsProto* const linear_constraints_;
  // NOT OWNED
  const SparseDoubleMatrixProto* const linear_constraint_matrix_;
  // An index into linear_constraints_, the constraint currently being viewed,
  // or Size(linear_constraints_) when IsDone().
  int current_con_ = 0;

  // Informal: the interval [matrix_start_, matrix_end_) gives the indices in
  // linear_constraint_matrix_ for linear_constraints_[current_con_]
  //
  // Invariant: if !IsDone():
  //   * matrix_start_: the first index in linear_constraint_matrix_ with row id
  //     >= RowId(linear_constraints_[current_con_])
  //   * matrix_end_: the first index in linear_constraint_matrix_ with row id
  //     >= RowId(linear_constraints_[current_con_]) + 1
  //
  // Implementation note: matrix_start_ and matrix_end_ equal
  // Size(linear_constraint_matrix_) when IsDone().
  int matrix_start_ = 0;
  int matrix_end_ = 0;
};

inline GScipVarType GScipVarTypeFromIsInteger(const bool is_integer) {
  return is_integer ? GScipVarType::kInteger : GScipVarType::kContinuous;
}

template <typename T>
SparseDoubleVectorProto FillSparseDoubleVector(
    const gtl::linked_hash_map<int64_t, T>& id_map,
    const absl::flat_hash_map<T, double>& value_map,
    const SparseVectorFilterProto& filter) {
  SparseVectorFilterPredicate predicate(filter);
  SparseDoubleVectorProto result;
  for (const auto [mathopt_id, scip_id] : id_map) {
    const double value = value_map.at(scip_id);
    if (predicate.AcceptsAndUpdate(mathopt_id, value)) {
      result.add_ids(mathopt_id);
      result.add_values(value);
    }
  }
  return result;
}

}  // namespace

absl::Status GScipSolver::AddVariables(
    const VariablesProto& variables,
    const absl::flat_hash_map<int64_t, double>& linear_objective_coefficients) {
  for (int i = 0; i < NumVariables(variables); ++i) {
    const int64_t id = SafeId(variables, i);
    // SCIP is failing with an assert in SCIPcreateVar() when input bounds are
    // inverted. That said, it is not an issue if the bounds are created
    // non-inverted and later changed. Thus here we use this hack to bypass the
    // assertion in this corner case.
    const bool inverted_bounds =
        variables.lower_bounds(i) > variables.upper_bounds(i);
    ASSIGN_OR_RETURN(
        SCIP_VAR* const v,
        gscip_->AddVariable(
            variables.lower_bounds(i),
            inverted_bounds ? variables.lower_bounds(i)
                            : variables.upper_bounds(i),
            gtl::FindWithDefault(linear_objective_coefficients, id),
            GScipVarTypeFromIsInteger(variables.integers(i)),
            SafeName(variables, i)));
    if (inverted_bounds) {
      const double ub = variables.upper_bounds(i);
      if (gscip_->VarType(v) == GScipVarType::kBinary && ub != 0.0 &&
          ub != 1.0) {
        // gSCIP (and SCIP actually) upgrades the variable type to kBinary if
        // the bounds passed to AddVariable() are both in {0.0, 1.0}. Changing
        // the bounds then raises an assertion in SCIP if the bounds is not in
        // {0.0, 1.0}.
        RETURN_IF_ERROR(gscip_->SetVarType(v, GScipVarType::kInteger));
      }
      RETURN_IF_ERROR(gscip_->SetUb(v, variables.upper_bounds(i)));
    }
    gtl::InsertOrDie(&variables_, id, v);
  }
  return absl::OkStatus();
}

absl::StatusOr<bool> GScipSolver::UpdateVariables(
    const VariableUpdatesProto& variable_updates) {
  for (const auto [id, is_integer] : MakeView(variable_updates.integers())) {
    // We intentionally update vartype first to ensure the checks below against
    // binary variables are against the up-to-date model.
    SCIP_VAR* const var = variables_.at(id);
    if (gscip_->VarType(var) == GScipVarType::kBinary) {
      // We reject bound updates on binary variables as they can lead to
      // crashes, or unexpected round-trip values if the vartype changes.
      return false;
    }
    RETURN_IF_ERROR(
        gscip_->SetVarType(var, GScipVarTypeFromIsInteger(is_integer)));
  }
  for (const auto [id, lb] : MakeView(variable_updates.lower_bounds())) {
    SCIP_VAR* const var = variables_.at(id);
    if (gscip_->VarType(var) == GScipVarType::kBinary) {
      // We reject bound updates on binary variables as they can lead to
      // crashes, or unexpected round-trip values if the vartype changes.
      return false;
    }
    RETURN_IF_ERROR(gscip_->SetLb(var, lb));
  }
  for (const auto [id, ub] : MakeView(variable_updates.upper_bounds())) {
    SCIP_VAR* const var = variables_.at(id);
    if (gscip_->VarType(var) == GScipVarType::kBinary) {
      // We reject bound updates on binary variables as they can lead to
      // crashes, or unexpected round-trip values if the vartype changes.
      return false;
    }
    RETURN_IF_ERROR(gscip_->SetUb(var, ub));
  }
  return true;
}

// SCIP does not natively support quadratic objectives, so we formulate them
// using quadratic constraints. We use a epi-/hypo-graph formulation depending
// on the objective sense:
//   min x'Qx <--> min y s.t. y >= x'Qx
//   max x'Qx <--> max y s.t. y <= x'Qx
absl::Status GScipSolver::AddQuadraticObjectiveTerms(
    const SparseDoubleMatrixProto& new_qp_terms, const bool maximize) {
  const int num_qp_terms = new_qp_terms.row_ids_size();
  if (num_qp_terms == 0) {
    return absl::OkStatus();
  }
  ASSIGN_OR_RETURN(
      SCIP_VAR* const qp_auxiliary_variable,
      gscip_->AddVariable(-kInf, kInf, 1.0, GScipVarType::kContinuous));
  GScipQuadraticRange range{
      .lower_bound = maximize ? 0.0 : -kInf,
      .linear_variables = {qp_auxiliary_variable},
      .linear_coefficients = {-1.0},
      .upper_bound = maximize ? kInf : 0.0,
  };
  range.quadratic_variables1.reserve(num_qp_terms);
  range.quadratic_variables2.reserve(num_qp_terms);
  range.quadratic_coefficients.reserve(num_qp_terms);
  for (int i = 0; i < num_qp_terms; ++i) {
    range.quadratic_variables1.push_back(
        variables_.at(new_qp_terms.row_ids(i)));
    range.quadratic_variables2.push_back(
        variables_.at(new_qp_terms.column_ids(i)));
    range.quadratic_coefficients.push_back(new_qp_terms.coefficients(i));
  }
  RETURN_IF_ERROR(gscip_->AddQuadraticConstraint(range).status());
  has_quadratic_objective_ = true;
  return absl::OkStatus();
}

absl::Status GScipSolver::AddLinearConstraints(
    const LinearConstraintsProto& linear_constraints,
    const SparseDoubleMatrixProto& linear_constraint_matrix) {
  for (LinearConstraintIterator lin_con_it(&linear_constraints,
                                           &linear_constraint_matrix);
       !lin_con_it.IsDone(); lin_con_it.Next()) {
    const LinearConstraintView current = lin_con_it.Current();

    GScipLinearRange range;
    range.lower_bound = current.lower_bound;
    range.upper_bound = current.upper_bound;
    range.coefficients = std::vector<double>(current.coefficients.begin(),
                                             current.coefficients.end());
    range.variables.reserve(current.variable_ids.size());
    for (const int64_t var_id : current.variable_ids) {
      range.variables.push_back(variables_.at(var_id));
    }
    ASSIGN_OR_RETURN(
        SCIP_CONS* const scip_con,
        gscip_->AddLinearConstraint(range, std::string(current.name)));
    gtl::InsertOrDie(&linear_constraints_, current.linear_constraint_id,
                     scip_con);
  }
  return absl::OkStatus();
}

absl::Status GScipSolver::UpdateLinearConstraints(
    const LinearConstraintUpdatesProto linear_constraint_updates,
    const SparseDoubleMatrixProto& linear_constraint_matrix,
    const std::optional<int64_t> first_new_var_id,
    const std::optional<int64_t> first_new_cstr_id) {
  for (const auto [id, lb] :
       MakeView(linear_constraint_updates.lower_bounds())) {
    RETURN_IF_ERROR(
        gscip_->SetLinearConstraintLb(linear_constraints_.at(id), lb));
  }
  for (const auto [id, ub] :
       MakeView(linear_constraint_updates.upper_bounds())) {
    RETURN_IF_ERROR(
        gscip_->SetLinearConstraintUb(linear_constraints_.at(id), ub));
  }
  for (const auto& [lin_con_id, var_coeffs] : SparseSubmatrixByRows(
           linear_constraint_matrix, /*start_row_id=*/0,
           /*end_row_id=*/first_new_cstr_id, /*start_col_id=*/0,
           /*end_col_id=*/first_new_var_id)) {
    for (const auto& [var_id, value] : var_coeffs) {
      RETURN_IF_ERROR(gscip_->SetLinearConstraintCoef(
          linear_constraints_.at(lin_con_id), variables_.at(var_id), value));
    }
  }
  return absl::OkStatus();
}

absl::Status GScipSolver::AddQuadraticConstraints(
    const google::protobuf::Map<int64_t, QuadraticConstraintProto>&
        quadratic_constraints) {
  for (const auto& [id, constraint] : quadratic_constraints) {
    GScipQuadraticRange range{
        .lower_bound = constraint.lower_bound(),
        .upper_bound = constraint.upper_bound(),
    };
    {
      const int num_linear_terms = constraint.linear_terms().ids_size();
      range.linear_variables.reserve(num_linear_terms);
      range.linear_coefficients.reserve(num_linear_terms);
      for (const auto [var_id, coeff] : MakeView(constraint.linear_terms())) {
        range.linear_variables.push_back(variables_.at(var_id));
        range.linear_coefficients.push_back(coeff);
      }
    }
    {
      const SparseDoubleMatrixProto& quad_terms = constraint.quadratic_terms();
      const int num_quad_terms = constraint.quadratic_terms().row_ids_size();
      range.quadratic_variables1.reserve(num_quad_terms);
      range.quadratic_variables2.reserve(num_quad_terms);
      range.quadratic_coefficients.reserve(num_quad_terms);
      for (int i = 0; i < num_quad_terms; ++i) {
        range.quadratic_variables1.push_back(
            variables_.at(quad_terms.row_ids(i)));
        range.quadratic_variables2.push_back(
            variables_.at(quad_terms.column_ids(i)));
        range.quadratic_coefficients.push_back(quad_terms.coefficients(i));
      }
    }
    ASSIGN_OR_RETURN(SCIP_CONS* const scip_con,
                     gscip_->AddQuadraticConstraint(range, constraint.name()));
    gtl::InsertOrDie(&quadratic_constraints_, id, scip_con);
  }
  return absl::OkStatus();
}

absl::Status GScipSolver::AddIndicatorConstraints(
    const google::protobuf::Map<int64_t, IndicatorConstraintProto>&
        indicator_constraints) {
  for (const auto& [id, constraint] : indicator_constraints) {
    if (!constraint.has_indicator_id()) {
      gtl::InsertOrDie(&indicator_constraints_, id, std::nullopt);
      continue;
    }
    SCIP_VAR* const indicator_var = variables_.at(constraint.indicator_id());
    // TODO(b/254860940): Properly handle the auxiliary variable that gSCIP may
    // add if `activate_on_zero()` is true and the indicator constraint is
    // deleted.
    GScipIndicatorConstraint data{
        .indicator_variable = indicator_var,
        .negate_indicator = constraint.activate_on_zero(),
    };
    const double lb = constraint.lower_bound();
    const double ub = constraint.upper_bound();
    if (lb > -kInf && ub < kInf) {
      return util::InvalidArgumentErrorBuilder()
             << "gSCIP does not support indicator constraints with ranged "
                "implied constraints; bounds are: "
             << lb << " <= ... <= " << ub;
    }
    // SCIP only supports implied constraints of the form ax <= b, so we must
    // formulate any constraints of the form cx >= d as -cx <= -d.
    double scaling;
    if (ub < kInf) {
      data.upper_bound = ub;
      scaling = 1.0;
    } else {
      data.upper_bound = -lb;
      scaling = -1.0;
    }
    {
      const int num_terms = constraint.expression().ids_size();
      data.variables.reserve(num_terms);
      data.coefficients.reserve(num_terms);
      for (const auto [var_id, coeff] : MakeView(constraint.expression())) {
        data.variables.push_back(variables_.at(var_id));
        data.coefficients.push_back(scaling * coeff);
      }
    }
    ASSIGN_OR_RETURN(SCIP_CONS* const scip_con,
                     gscip_->AddIndicatorConstraint(data, constraint.name()));
    gtl::InsertOrDie(&indicator_constraints_, id,
                     std::make_pair(scip_con, constraint.indicator_id()));
  }
  return absl::OkStatus();
}

absl::StatusOr<std::pair<SCIP_VAR*, SCIP_CONS*>>
GScipSolver::AddSlackVariableEqualToExpression(
    const LinearExpressionProto& expression) {
  ASSIGN_OR_RETURN(
      SCIP_VAR * aux_var,
      gscip_->AddVariable(-kInf, kInf, 0.0, GScipVarType::kContinuous));
  GScipLinearRange range{
      .lower_bound = -expression.offset(),
      .upper_bound = -expression.offset(),
  };
  range.variables.push_back(aux_var);
  range.coefficients.push_back(-1.0);
  for (int i = 0; i < expression.ids_size(); ++i) {
    range.variables.push_back(variables_.at(expression.ids(i)));
    range.coefficients.push_back(expression.coefficients(i));
  }
  ASSIGN_OR_RETURN(SCIP_CONS * aux_constr, gscip_->AddLinearConstraint(range));
  return std::make_pair(aux_var, aux_constr);
}

absl::Status GScipSolver::AuxiliaryStructureHandler::DeleteStructure(
    GScip& gscip) {
  for (SCIP_CONS* const constraint : constraints) {
    RETURN_IF_ERROR(gscip.DeleteConstraint(constraint));
  }
  for (SCIP_VAR* const variable : variables) {
    RETURN_IF_ERROR(gscip.DeleteVariable(variable));
  }
  variables.clear();
  constraints.clear();
  return absl::OkStatus();
}

absl::StatusOr<std::pair<GScipSOSData, GScipSolver::AuxiliaryStructureHandler>>
GScipSolver::ProcessSosProto(const SosConstraintProto& sos_constraint) {
  GScipSOSData data;
  AuxiliaryStructureHandler handler;
  absl::flat_hash_set<int64_t> reused_variables;
  for (const LinearExpressionProto& expr : sos_constraint.expressions()) {
    // If the expression is equivalent to 1 * some_variable, there is no need to
    // add a slack variable. But, SCIP crashes in debug mode if you repeat a
    // variable twice within an SOS constraint, so we track the ones we reuse.
    if (expr.ids_size() == 1 && expr.coefficients(0) == 1.0 &&
        expr.offset() == 0.0 && !reused_variables.contains(expr.ids(0))) {
      data.variables.push_back(variables_.at(expr.ids(0)));
      reused_variables.insert(expr.ids(0));
    } else {
      ASSIGN_OR_RETURN((const auto [slack_var, slack_constr]),
                       AddSlackVariableEqualToExpression(expr));
      handler.variables.push_back(slack_var);
      handler.constraints.push_back(slack_constr);
      data.variables.push_back(slack_var);
    }
  }
  for (const double weight : sos_constraint.weights()) {
    data.weights.push_back(weight);
  }
  return std::make_pair(data, handler);
}

absl::Status GScipSolver::AddSos1Constraints(
    const google::protobuf::Map<int64_t, SosConstraintProto>&
        sos1_constraints) {
  for (const auto& [id, constraint] : sos1_constraints) {
    ASSIGN_OR_RETURN((auto [sos_data, slack_handler]),
                     ProcessSosProto(constraint));
    ASSIGN_OR_RETURN(
        SCIP_CONS* const scip_con,
        gscip_->AddSOS1Constraint(std::move(sos_data), constraint.name()));
    gtl::InsertOrDie(&sos1_constraints_, id,
                     std::make_pair(scip_con, std::move(slack_handler)));
  }
  return absl::OkStatus();
}

absl::Status GScipSolver::AddSos2Constraints(
    const google::protobuf::Map<int64_t, SosConstraintProto>&
        sos2_constraints) {
  for (const auto& [id, constraint] : sos2_constraints) {
    ASSIGN_OR_RETURN((auto [sos_data, slack_handler]),
                     ProcessSosProto(constraint));
    ASSIGN_OR_RETURN(SCIP_CONS* const scip_con,
                     gscip_->AddSOS2Constraint(sos_data, constraint.name()));
    gtl::InsertOrDie(&sos2_constraints_, id,
                     std::make_pair(scip_con, std::move(slack_handler)));
  }
  return absl::OkStatus();
}

GScipParameters::MetaParamValue ConvertMathOptEmphasis(EmphasisProto emphasis) {
  switch (emphasis) {
    case EMPHASIS_OFF:
      return GScipParameters::OFF;
    case EMPHASIS_LOW:
      return GScipParameters::FAST;
    case EMPHASIS_MEDIUM:
    case EMPHASIS_UNSPECIFIED:
      return GScipParameters::DEFAULT_META_PARAM_VALUE;
    case EMPHASIS_HIGH:
    case EMPHASIS_VERY_HIGH:
      return GScipParameters::AGGRESSIVE;
    default:
      LOG(FATAL) << "Unsupported MathOpt Emphasis value: "
                 << ProtoEnumToString(emphasis)
                 << " unknown, error setting gSCIP parameters";
  }
}

absl::StatusOr<GScipParameters> GScipSolver::MergeParameters(
    const SolveParametersProto& solve_parameters) {
  // First build the result by translating common parameters to a
  // GScipParameters, and then merging with user provided gscip_parameters.
  // This results in user provided solver specific parameters overwriting
  // common parameters should there be any conflict.
  GScipParameters result;
  std::vector<std::string> warnings;

  // By default SCIP catches Ctrl-C but we don't want this behavior when the
  // users uses SCIP through MathOpt.
  GScipSetCatchCtrlC(false, &result);

  if (solve_parameters.has_time_limit()) {
    GScipSetTimeLimit(
        util_time::DecodeGoogleApiProto(solve_parameters.time_limit()).value(),
        &result);
  }
  if (solve_parameters.has_iteration_limit()) {
    warnings.push_back("parameter iteration_limit not supported for gSCIP.");
  }
  if (solve_parameters.has_node_limit()) {
    (*result.mutable_long_params())["limits/totalnodes"] =
        solve_parameters.node_limit();
  }
  if (solve_parameters.has_cutoff_limit()) {
    result.set_objective_limit(solve_parameters.cutoff_limit());
  }
  if (solve_parameters.has_objective_limit()) {
    warnings.push_back("parameter objective_limit not supported for gSCIP.");
  }
  if (solve_parameters.has_best_bound_limit()) {
    warnings.push_back("parameter best_bound_limit not supported for gSCIP.");
  }
  if (solve_parameters.has_solution_limit()) {
    (*result.mutable_int_params())["limits/solutions"] =
        solve_parameters.solution_limit();
  }
  // GScip has also GScipSetOutputEnabled() but this changes the log
  // level. Setting `silence_output` sets the `quiet` field on the default
  // message handler of SCIP which removes the output. Here it is important to
  // use this rather than changing the log level so that if the user provides
  // a MessageCallback function they do get some messages even when
  // `enable_output` is false.
  result.set_silence_output(!solve_parameters.enable_output());
  if (solve_parameters.has_threads()) {
    GScipSetMaxNumThreads(solve_parameters.threads(), &result);
  }
  if (solve_parameters.has_random_seed()) {
    GScipSetRandomSeed(&result, solve_parameters.random_seed());
  }
  if (solve_parameters.has_absolute_gap_tolerance()) {
    (*result.mutable_real_params())["limits/absgap"] =
        solve_parameters.absolute_gap_tolerance();
  }
  if (solve_parameters.has_relative_gap_tolerance()) {
    (*result.mutable_real_params())["limits/gap"] =
        solve_parameters.relative_gap_tolerance();
  }
  if (solve_parameters.has_solution_pool_size()) {
    result.set_num_solutions(solve_parameters.solution_pool_size());
    // We must set limits/maxsol (the internal solution pool) and
    // limits/maxorigsol (the number solutions to attempt to transform back to
    // the user.
    //
    // NOTE: As of SCIP 8, limits/maxsol defaults to 100.
    (*result.mutable_int_params())["limits/maxsol"] =
        std::max(100, solve_parameters.solution_pool_size());
    (*result.mutable_int_params())["limits/maxorigsol"] =
        solve_parameters.solution_pool_size();
  }

  if (solve_parameters.lp_algorithm() != LP_ALGORITHM_UNSPECIFIED) {
    char alg = 's';
    switch (solve_parameters.lp_algorithm()) {
      case LP_ALGORITHM_PRIMAL_SIMPLEX:
        alg = 'p';
        break;
      case LP_ALGORITHM_DUAL_SIMPLEX:
        alg = 'd';
        break;
      case LP_ALGORITHM_BARRIER:
        // As SCIP is configured in ortools, this is an error, since we are not
        // connected to any LP solver that runs barrier.
        warnings.push_back(
            "parameter lp_algorithm with value BARRIER is not supported for "
            "gSCIP in ortools.");
        alg = 'c';
        break;
      case LP_ALGORITHM_FIRST_ORDER:
        warnings.push_back(
            "parameter lp_algorithm with value FIRST_ORDER is not supported.");
        break;
      default:
        LOG(FATAL) << "LPAlgorithm: "
                   << ProtoEnumToString(solve_parameters.lp_algorithm())
                   << " unknown, error setting gSCIP parameters";
    }
    (*result.mutable_char_params())["lp/initalgorithm"] = alg;
  }
  if (solve_parameters.presolve() != EMPHASIS_UNSPECIFIED) {
    result.set_presolve(ConvertMathOptEmphasis(solve_parameters.presolve()));
  }
  if (solve_parameters.cuts() != EMPHASIS_UNSPECIFIED) {
    result.set_separating(ConvertMathOptEmphasis(solve_parameters.cuts()));
  }
  if (solve_parameters.heuristics() != EMPHASIS_UNSPECIFIED) {
    result.set_heuristics(
        ConvertMathOptEmphasis(solve_parameters.heuristics()));
  }
  if (solve_parameters.scaling() != EMPHASIS_UNSPECIFIED) {
    int scaling_value;
    switch (solve_parameters.scaling()) {
      case EMPHASIS_OFF:
        scaling_value = 0;
        break;
      case EMPHASIS_LOW:
      case EMPHASIS_MEDIUM:
        scaling_value = 1;
        break;
      case EMPHASIS_HIGH:
      case EMPHASIS_VERY_HIGH:
        scaling_value = 2;
        break;
      default:
        LOG(FATAL) << "Scaling emphasis: "
                   << ProtoEnumToString(solve_parameters.scaling())
                   << " unknown, error setting gSCIP parameters";
    }
    (*result.mutable_int_params())["lp/scaling"] = scaling_value;
  }

  result.MergeFrom(solve_parameters.gscip());

  if (!warnings.empty()) {
    return absl::InvalidArgumentError(absl::StrJoin(warnings, "; "));
  }
  return result;
}

namespace {

std::string JoinDetails(absl::string_view gscip_detail,
                        absl::string_view math_opt_detail) {
  if (gscip_detail.empty()) {
    return std::string(math_opt_detail);
  }
  if (math_opt_detail.empty()) {
    return std::string(gscip_detail);
  }
  return absl::StrCat(gscip_detail, "; ", math_opt_detail);
}

double GetBestPrimalObjective(
    const double gscip_best_objective,
    const google::protobuf::RepeatedPtrField<SolutionProto>& solutions,
    const bool is_maximize) {
  double result = gscip_best_objective;
  for (const auto& solution : solutions) {
    if (solution.has_primal_solution() &&
        solution.primal_solution().feasibility_status() ==
            SOLUTION_STATUS_FEASIBLE) {
      result =
          is_maximize
              ? std::max(result, solution.primal_solution().objective_value())
              : std::min(result, solution.primal_solution().objective_value());
    }
  }
  return result;
}

absl::StatusOr<TerminationProto> ConvertTerminationReason(
    const GScipResult& gscip_result, const bool is_maximize,
    const google::protobuf::RepeatedPtrField<SolutionProto>& solutions,
    const bool had_cutoff) {
  const bool has_feasible_solution = !solutions.empty();
  const GScipOutput::Status gscip_status = gscip_result.gscip_output.status();
  absl::string_view gscip_status_detail =
      gscip_result.gscip_output.status_detail();
  const GScipSolvingStats& gscip_stats = gscip_result.gscip_output.stats();
  // SCIP may return primal solutions that are slightly better than
  // gscip_stats.best_objective().
  const double best_primal_objective = GetBestPrimalObjective(
      gscip_stats.best_objective(), solutions, is_maximize);
  const std::optional<double> optional_finite_primal_objective =
      has_feasible_solution ? std::make_optional(best_primal_objective)
                            : std::nullopt;
  // For SCIP, the only indicator for the existence of a dual feasible solution
  // is a finite dual bound.
  const std::optional<double> optional_dual_objective =
      std::isfinite(gscip_stats.best_bound())
          ? std::make_optional(gscip_stats.best_bound())
          : std::nullopt;
  switch (gscip_status) {
    case GScipOutput::USER_INTERRUPT:
      return LimitTerminationProto(
          is_maximize, LIMIT_INTERRUPTED, optional_finite_primal_objective,
          optional_dual_objective,
          JoinDetails(gscip_status_detail,
                      "underlying gSCIP status: USER_INTERRUPT"));
    case GScipOutput::NODE_LIMIT:
      return LimitTerminationProto(
          is_maximize, LIMIT_NODE, optional_finite_primal_objective,
          optional_dual_objective,
          JoinDetails(gscip_status_detail,
                      "underlying gSCIP status: NODE_LIMIT"));
    case GScipOutput::TOTAL_NODE_LIMIT:
      return LimitTerminationProto(
          is_maximize, LIMIT_NODE, optional_finite_primal_objective,
          optional_dual_objective,
          JoinDetails(gscip_status_detail,
                      "underlying gSCIP status: TOTAL_NODE_LIMIT"));
    case GScipOutput::STALL_NODE_LIMIT:
      return LimitTerminationProto(
          is_maximize, LIMIT_SLOW_PROGRESS, optional_finite_primal_objective,
          optional_dual_objective, gscip_status_detail);
    case GScipOutput::TIME_LIMIT:
      return LimitTerminationProto(
          is_maximize, LIMIT_TIME, optional_finite_primal_objective,
          optional_dual_objective, gscip_status_detail);
    case GScipOutput::MEM_LIMIT:
      return LimitTerminationProto(
          is_maximize, LIMIT_MEMORY, optional_finite_primal_objective,
          optional_dual_objective, gscip_status_detail);
    case GScipOutput::SOL_LIMIT:
      return LimitTerminationProto(
          is_maximize, LIMIT_SOLUTION, optional_finite_primal_objective,
          optional_dual_objective,
          JoinDetails(gscip_status_detail,
                      "underlying gSCIP status: SOL_LIMIT"));
    case GScipOutput::BEST_SOL_LIMIT:
      return LimitTerminationProto(
          is_maximize, LIMIT_SOLUTION, optional_finite_primal_objective,
          optional_dual_objective,
          JoinDetails(gscip_status_detail,
                      "underlying gSCIP status: BEST_SOL_LIMIT"));
    case GScipOutput::RESTART_LIMIT:
      return LimitTerminationProto(
          is_maximize, LIMIT_OTHER, optional_finite_primal_objective,
          optional_dual_objective,
          JoinDetails(gscip_status_detail,
                      "underlying gSCIP status: RESTART_LIMIT"));
    case GScipOutput::OPTIMAL:
      return OptimalTerminationProto(
          /*finite_primal_objective=*/best_primal_objective,
          /*dual_objective=*/gscip_stats.best_bound(),
          JoinDetails(gscip_status_detail, "underlying gSCIP status: OPTIMAL"));
    case GScipOutput::GAP_LIMIT:
      return OptimalTerminationProto(
          /*finite_primal_objective=*/best_primal_objective,
          /*dual_objective=*/gscip_stats.best_bound(),
          JoinDetails(gscip_status_detail,
                      "underlying gSCIP status: GAP_LIMIT"));
    case GScipOutput::INFEASIBLE:
      if (had_cutoff) {
        return CutoffTerminationProto(is_maximize, gscip_status_detail);
      } else {
        // By convention infeasible MIPs are always dual feasible.
        const FeasibilityStatusProto dual_feasibility_status =
            FEASIBILITY_STATUS_FEASIBLE;
        return InfeasibleTerminationProto(is_maximize, dual_feasibility_status,
                                          gscip_status_detail);
      }
    case GScipOutput::UNBOUNDED: {
      if (has_feasible_solution) {
        return UnboundedTerminationProto(
            is_maximize,
            JoinDetails(gscip_status_detail,
                        "underlying gSCIP status was UNBOUNDED, both primal "
                        "ray and feasible solution are present"));
      } else {
        return InfeasibleOrUnboundedTerminationProto(
            is_maximize,
            /*dual_feasibility_status=*/FEASIBILITY_STATUS_INFEASIBLE,
            JoinDetails(
                gscip_status_detail,
                "underlying gSCIP status was UNBOUNDED, but only primal ray "
                "was given, no feasible solution was found"));
      }
    }
    case GScipOutput::INF_OR_UNBD:
      return InfeasibleOrUnboundedTerminationProto(
          is_maximize,
          /*dual_feasibility_status=*/FEASIBILITY_STATUS_UNDETERMINED,
          JoinDetails(gscip_status_detail,
                      "underlying gSCIP status: INF_OR_UNBD"));
    case GScipOutput::TERMINATE:
      return LimitTerminationProto(
          is_maximize, LIMIT_INTERRUPTED, optional_finite_primal_objective,
          optional_dual_objective,
          JoinDetails(gscip_status_detail,
                      "underlying gSCIP status: TERMINATE"));
    case GScipOutput::INVALID_SOLVER_PARAMETERS:
      return absl::InvalidArgumentError(gscip_status_detail);
    case GScipOutput::UNKNOWN:
      return absl::InternalError(JoinDetails(
          gscip_status_detail, "Unexpected GScipOutput.status: UNKNOWN"));
    default:
      return absl::InternalError(JoinDetails(
          gscip_status_detail, absl::StrCat("Missing GScipOutput.status case: ",
                                            ProtoEnumToString(gscip_status))));
  }
}

}  // namespace

absl::StatusOr<SolveResultProto> GScipSolver::CreateSolveResultProto(
    GScipResult gscip_result, const ModelSolveParametersProto& model_parameters,
    const std::optional<double> cutoff) {
  SolveResultProto solve_result;
  const bool is_maximize = gscip_->ObjectiveIsMaximize();
  // When an objective limit is set, SCIP returns the solutions worse than the
  // limit, we need to filter these out manually.
  const auto meets_cutoff = [cutoff, is_maximize](const double obj_value) {
    if (!cutoff.has_value()) {
      return true;
    }
    if (is_maximize) {
      return obj_value >= *cutoff;
    } else {
      return obj_value <= *cutoff;
    }
  };

  CHECK_EQ(gscip_result.solutions.size(), gscip_result.objective_values.size());
  for (int i = 0; i < gscip_result.solutions.size(); ++i) {
    // GScip ensures the solutions are returned best objective first.
    if (!meets_cutoff(gscip_result.objective_values[i])) {
      break;
    }
    SolutionProto* const solution = solve_result.add_solutions();
    PrimalSolutionProto* const primal_solution =
        solution->mutable_primal_solution();
    primal_solution->set_objective_value(gscip_result.objective_values[i]);
    primal_solution->set_feasibility_status(SOLUTION_STATUS_FEASIBLE);
    *primal_solution->mutable_variable_values() =
        FillSparseDoubleVector(variables_, gscip_result.solutions[i],
                               model_parameters.variable_values_filter());
  }
  if (!gscip_result.primal_ray.empty()) {
    *solve_result.add_primal_rays()->mutable_variable_values() =
        FillSparseDoubleVector(variables_, gscip_result.primal_ray,
                               model_parameters.variable_values_filter());
  }
  ASSIGN_OR_RETURN(*solve_result.mutable_termination(),
                   ConvertTerminationReason(gscip_result, is_maximize,
                                            solve_result.solutions(),
                                            /*had_cutoff=*/cutoff.has_value()));
  SolveStatsProto* const common_stats = solve_result.mutable_solve_stats();
  const GScipSolvingStats& gscip_stats = gscip_result.gscip_output.stats();

  common_stats->set_node_count(gscip_stats.node_count());
  common_stats->set_simplex_iterations(gscip_stats.primal_simplex_iterations() +
                                       gscip_stats.dual_simplex_iterations());
  common_stats->set_barrier_iterations(gscip_stats.barrier_iterations());
  *solve_result.mutable_gscip_output() = std::move(gscip_result.gscip_output);
  return solve_result;
}

GScipSolver::GScipSolver(std::unique_ptr<GScip> gscip)
    : gscip_(std::move(ABSL_DIE_IF_NULL(gscip))) {}

absl::StatusOr<std::unique_ptr<SolverInterface>> GScipSolver::New(
    const ModelProto& model, const InitArgs&) {
  RETURN_IF_ERROR(ModelIsSupported(model, kGscipSupportedStructures, "SCIP"));
  ASSIGN_OR_RETURN(std::unique_ptr<GScip> gscip, GScip::Create(model.name()));
  RETURN_IF_ERROR(gscip->SetMaximize(model.objective().maximize()));
  RETURN_IF_ERROR(gscip->SetObjectiveOffset(model.objective().offset()));
  auto solver = absl::WrapUnique(new GScipSolver(std::move(gscip)));
  RETURN_IF_ERROR(solver->constraint_handler_.Register(solver->gscip_.get()));
  RETURN_IF_ERROR(solver->AddVariables(
      model.variables(),
      SparseDoubleVectorAsMap(model.objective().linear_coefficients())));
  RETURN_IF_ERROR(solver->AddQuadraticObjectiveTerms(
      model.objective().quadratic_coefficients(),
      model.objective().maximize()));
  RETURN_IF_ERROR(solver->AddLinearConstraints(
      model.linear_constraints(), model.linear_constraint_matrix()));
  RETURN_IF_ERROR(
      solver->AddQuadraticConstraints(model.quadratic_constraints()));
  RETURN_IF_ERROR(
      solver->AddIndicatorConstraints(model.indicator_constraints()));
  RETURN_IF_ERROR(solver->AddSos1Constraints(model.sos1_constraints()));
  RETURN_IF_ERROR(solver->AddSos2Constraints(model.sos2_constraints()));
  return solver;
}

absl::StatusOr<SolveResultProto> GScipSolver::Solve(
    const SolveParametersProto& parameters,
    const ModelSolveParametersProto& model_parameters,
    const MessageCallback message_cb,
    const CallbackRegistrationProto& callback_registration, Callback cb,
    const SolveInterrupter* const interrupter) {
  RETURN_IF_ERROR(ModelSolveParametersAreSupported(
      model_parameters, kGscipSupportedStructures, "SCIP"));
  const absl::Time start = absl::Now();

  GScip::Interrupter gscip_interrupter;
  const ScopedSolveInterrupterCallback scoped_interrupt_cb(
      interrupter, [&]() { gscip_interrupter.Interrupt(); });
  const bool use_interrupter = interrupter != nullptr || cb != nullptr;

  RETURN_IF_ERROR(CheckRegisteredCallbackEvents(
      callback_registration,
      /*supported_events=*/{CALLBACK_EVENT_MIP_SOLUTION,
                            CALLBACK_EVENT_MIP_NODE}));
  if (constraint_data_ != nullptr) {
    return absl::InternalError(
        "constraint_data_ should always be null at the start of "
        "GScipSolver::Solver()");
  }
  SCIP_CONS* callback_cons = nullptr;
  if (cb != nullptr) {
    // NOTE: we must meet the invariant on GScipSolverConstraintData, that when
    // user_callback != nullptr, all fields are filled in.
    constraint_data_ = std::make_unique<GScipSolverConstraintData>();
    constraint_data_->user_callback = std::move(cb);
    constraint_data_->SetWhenRunAndAdds(callback_registration);
    constraint_data_->solve_start_time = absl::Now();
    constraint_data_->variables = &variables_;
    constraint_data_->variable_node_filter =
        &callback_registration.mip_node_filter();
    constraint_data_->variable_solution_filter =
        &callback_registration.mip_solution_filter();
    constraint_data_->interrupter = &gscip_interrupter;
    // NOTE: it is critical that this constraint is added after all other
    // constraints, as otherwise, due to what appears to be a bug in SCIP, we
    // may run our callback before checking all the constraints in the model,
    // see https://listserv.zib.de/pipermail/scip/2023-November/004785.html.
    ASSIGN_OR_RETURN(callback_cons,
                     constraint_handler_.AddCallbackConstraint(
                         gscip_.get(), "mathopt_callback_constraint",
                         constraint_data_.get()));
  }
  const auto cleanup_constraint_data = absl::Cleanup([this]() {
    if (constraint_data_ != nullptr) {
      *constraint_data_ = {};
    }
  });

  BufferedMessageCallback buffered_message_callback(std::move(message_cb));
  auto message_cb_cleanup = absl::MakeCleanup(
      [&buffered_message_callback]() { buffered_message_callback.Flush(); });

  ASSIGN_OR_RETURN(auto gscip_parameters, MergeParameters(parameters));

  for (const SolutionHintProto& hint : model_parameters.solution_hints()) {
    absl::flat_hash_map<SCIP_VAR*, double> partial_solution;
    for (const auto [id, val] : MakeView(hint.variable_values())) {
      partial_solution.insert({variables_.at(id), val});
    }
    RETURN_IF_ERROR(gscip_->SuggestHint(partial_solution).status());
  }
  for (const auto [id, value] :
       MakeView(model_parameters.branching_priorities())) {
    RETURN_IF_ERROR(gscip_->SetBranchingPriority(variables_.at(id), value));
  }

  // SCIP returns "infeasible" when the model contain invalid bounds.
  RETURN_IF_ERROR(ListInvertedBounds().ToStatus());
  RETURN_IF_ERROR(ListInvalidIndicators().ToStatus());

  GScipMessageHandler gscip_msg_cb = nullptr;
  if (buffered_message_callback.has_user_message_callback()) {
    gscip_msg_cb = [&buffered_message_callback](
                       const auto, const absl::string_view message) {
      buffered_message_callback.OnMessage(message);
    };
  }

  ASSIGN_OR_RETURN(
      GScipResult gscip_result,
      gscip_->Solve(gscip_parameters, std::move(gscip_msg_cb),
                    use_interrupter ? &gscip_interrupter : nullptr));

  // Flush the potential last unfinished line.
  std::move(message_cb_cleanup).Invoke();

  if (callback_cons != nullptr) {
    RETURN_IF_ERROR(gscip_->DeleteConstraint(callback_cons));
    constraint_data_.reset();
  }

  ASSIGN_OR_RETURN(
      SolveResultProto result,
      CreateSolveResultProto(std::move(gscip_result), model_parameters,
                             parameters.has_cutoff_limit()
                                 ? std::make_optional(parameters.cutoff_limit())
                                 : std::nullopt));

  // Reset solve-specific model parameters so that they do not leak into the
  // next solve. Note that 0 is the default branching priority.
  for (const auto [id, unused] :
       MakeView(model_parameters.branching_priorities())) {
    RETURN_IF_ERROR(gscip_->SetBranchingPriority(variables_.at(id), 0));
  }

  CHECK_OK(util_time::EncodeGoogleApiProto(
      absl::Now() - start, result.mutable_solve_stats()->mutable_solve_time()));
  return result;
}

absl::flat_hash_set<SCIP_VAR*> GScipSolver::LookupAllVariables(
    absl::Span<const int64_t> variable_ids) {
  absl::flat_hash_set<SCIP_VAR*> result;
  result.reserve(variable_ids.size());
  for (const int64_t var_id : variable_ids) {
    result.insert(variables_.at(var_id));
  }
  return result;
}

// Returns the ids of variables and linear constraints with inverted bounds.
InvertedBounds GScipSolver::ListInvertedBounds() const {
  // Get the SCIP variables/constraints with inverted bounds.
  InvertedBounds inverted_bounds;
  for (const auto& [id, var] : variables_) {
    if (gscip_->Lb(var) > gscip_->Ub(var)) {
      inverted_bounds.variables.push_back(id);
    }
  }
  for (const auto& [id, cstr] : linear_constraints_) {
    if (gscip_->LinearConstraintLb(cstr) > gscip_->LinearConstraintUb(cstr)) {
      inverted_bounds.linear_constraints.push_back(id);
    }
  }

  // Above code have inserted ids in non-stable order.
  std::sort(inverted_bounds.variables.begin(), inverted_bounds.variables.end());
  std::sort(inverted_bounds.linear_constraints.begin(),
            inverted_bounds.linear_constraints.end());
  return inverted_bounds;
}

InvalidIndicators GScipSolver::ListInvalidIndicators() const {
  InvalidIndicators invalid_indicators;
  for (const auto& [constraint_id, gscip_data] : indicator_constraints_) {
    if (!gscip_data.has_value()) {
      continue;
    }
    const auto [gscip_constraint, indicator_id] = *gscip_data;
    SCIP_VAR* const indicator_var = variables_.at(indicator_id);
    if (gscip_->VarType(indicator_var) == GScipVarType::kContinuous ||
        gscip_->Lb(indicator_var) < 0.0 || gscip_->Ub(indicator_var) > 1.0) {
      invalid_indicators.invalid_indicators.push_back(
          {.variable = indicator_id, .constraint = constraint_id});
    }
  }
  invalid_indicators.Sort();
  return invalid_indicators;
}

absl::StatusOr<bool> GScipSolver::Update(const ModelUpdateProto& model_update) {
  if (!gscip_
           ->CanSafeBulkDelete(
               LookupAllVariables(model_update.deleted_variable_ids()))
           .ok() ||
      !UpdateIsSupported(model_update, kGscipSupportedStructures)) {
    return false;
  }
  // As of 2022-09-12 we do not support quadratic objective updates. Therefore,
  // if we already have a quadratic objective stored, we reject any update that
  // changes the objective sense (which would break the epi-/hypo-graph
  // formulation) or changes any quadratic objective coefficients.
  if (has_quadratic_objective_ &&
      (model_update.objective_updates().has_direction_update() ||
       model_update.objective_updates()
               .quadratic_coefficients()
               .row_ids_size() > 0)) {
    return false;
  }

  for (const int64_t constraint_id :
       model_update.deleted_linear_constraint_ids()) {
    SCIP_CONS* const scip_cons = linear_constraints_.at(constraint_id);
    linear_constraints_.erase(constraint_id);
    RETURN_IF_ERROR(gscip_->DeleteConstraint(scip_cons));
  }
  {
    const absl::flat_hash_set<SCIP_VAR*> vars_to_delete =
        LookupAllVariables(model_update.deleted_variable_ids());
    for (const int64_t deleted_variable_id :
         model_update.deleted_variable_ids()) {
      variables_.erase(deleted_variable_id);
    }
    RETURN_IF_ERROR(gscip_->SafeBulkDelete(vars_to_delete));
  }

  const std::optional<int64_t> first_new_var_id =
      FirstVariableId(model_update.new_variables());
  const std::optional<int64_t> first_new_cstr_id =
      FirstLinearConstraintId(model_update.new_linear_constraints());

  if (model_update.objective_updates().has_direction_update()) {
    RETURN_IF_ERROR(gscip_->SetMaximize(
        model_update.objective_updates().direction_update()));
  }
  if (model_update.objective_updates().has_offset_update()) {
    RETURN_IF_ERROR(gscip_->SetObjectiveOffset(
        model_update.objective_updates().offset_update()));
  }
  if (const auto response = UpdateVariables(model_update.variable_updates());
      !response.ok() || !(*response)) {
    return response;
  }
  const absl::flat_hash_map<int64_t, double> linear_objective_updates =
      SparseDoubleVectorAsMap(
          model_update.objective_updates().linear_coefficients());
  for (const auto& obj_pair : linear_objective_updates) {
    // New variables' coefficient is set when the variables are added below.
    if (!first_new_var_id.has_value() || obj_pair.first < *first_new_var_id) {
      RETURN_IF_ERROR(
          gscip_->SetObjCoef(variables_.at(obj_pair.first), obj_pair.second));
    }
  }

  // Here the model_update.linear_constraint_matrix_updates is split into three
  // sub-matrix:
  //
  //                existing    new
  //                columns   columns
  //              /         |         \
  //    existing  |    1    |    2    |
  //    rows      |         |         |
  //              |---------+---------|
  //    new       |                   |
  //    rows      |         3         |
  //              \                   /
  //
  // The coefficients of sub-matrix 1 are set by UpdateLinearConstraints(), the
  // ones of sub-matrix 2 by AddVariables() and the ones of the sub-matrix 3 by
  // AddLinearConstraints(). The rationale here is that SCIPchgCoefLinear() has
  // a complexity of O(non_zeros). Thus it is inefficient and can lead to O(n^2)
  // behaviors if it was used for new rows or for new columns. For new rows it
  // is more efficient to pass all the variables coefficients at once when
  // building the constraints. For new columns and existing rows, since we can
  // assume there is no existing coefficient, we can use SCIPaddCoefLinear()
  // which is O(1). This leads to only use SCIPchgCoefLinear() for changing the
  // coefficients of existing rows and columns.
  //
  // TODO(b/215722113): maybe we could use SCIPaddCoefLinear() for sub-matrix 1.

  // Add new variables.
  RETURN_IF_ERROR(
      AddVariables(model_update.new_variables(), linear_objective_updates));

  RETURN_IF_ERROR(AddQuadraticObjectiveTerms(
      model_update.objective_updates().quadratic_coefficients(),
      gscip_->ObjectiveIsMaximize()));

  // Update linear constraints properties and sub-matrix 1.
  RETURN_IF_ERROR(
      UpdateLinearConstraints(model_update.linear_constraint_updates(),
                              model_update.linear_constraint_matrix_updates(),
                              /*first_new_var_id=*/first_new_var_id,
                              /*first_new_cstr_id=*/first_new_cstr_id));

  // Update the sub-matrix 2.
  const std::optional first_new_variable_id =
      FirstVariableId(model_update.new_variables());
  if (first_new_variable_id.has_value()) {
    for (const auto& [lin_con_id, var_coeffs] :
         SparseSubmatrixByRows(model_update.linear_constraint_matrix_updates(),
                               /*start_row_id=*/0,
                               /*end_row_id=*/first_new_cstr_id,
                               /*start_col_id=*/*first_new_variable_id,
                               /*end_col_id=*/std::nullopt)) {
      for (const auto& [var_id, value] : var_coeffs) {
        // See above why we use AddLinearConstraintCoef().
        RETURN_IF_ERROR(gscip_->AddLinearConstraintCoef(
            linear_constraints_.at(lin_con_id), variables_.at(var_id), value));
      }
    }
  }

  // Add the new constraints and sets sub-matrix 3.
  RETURN_IF_ERROR(
      AddLinearConstraints(model_update.new_linear_constraints(),
                           model_update.linear_constraint_matrix_updates()));

  // Quadratic constraint updates.
  for (const int64_t constraint_id :
       model_update.quadratic_constraint_updates().deleted_constraint_ids()) {
    RETURN_IF_ERROR(gscip_->DeleteConstraint(
        quadratic_constraints_.extract(constraint_id).mapped()));
  }
  RETURN_IF_ERROR(AddQuadraticConstraints(
      model_update.quadratic_constraint_updates().new_constraints()));

  // Indicator constraint updates.
  for (const int64_t constraint_id :
       model_update.indicator_constraint_updates().deleted_constraint_ids()) {
    const auto gscip_data =
        indicator_constraints_.extract(constraint_id).mapped();
    if (!gscip_data.has_value()) {
      continue;
    }
    SCIP_CONS* const gscip_constraint = gscip_data->first;
    CHECK_NE(gscip_constraint, nullptr);
    RETURN_IF_ERROR(gscip_->DeleteConstraint(gscip_constraint));
  }
  RETURN_IF_ERROR(AddIndicatorConstraints(
      model_update.indicator_constraint_updates().new_constraints()));

  // SOS1 constraint updates.
  for (const int64_t constraint_id :
       model_update.sos1_constraint_updates().deleted_constraint_ids()) {
    auto [gscip_constraint, slack_handler] =
        sos1_constraints_.extract(constraint_id).mapped();
    RETURN_IF_ERROR(gscip_->DeleteConstraint(gscip_constraint));
    RETURN_IF_ERROR(slack_handler.DeleteStructure(*gscip_));
  }
  RETURN_IF_ERROR(AddSos1Constraints(
      model_update.sos1_constraint_updates().new_constraints()));

  // SOS2 constraint updates.
  for (const int64_t constraint_id :
       model_update.sos2_constraint_updates().deleted_constraint_ids()) {
    auto [gscip_constraint, slack_handler] =
        sos2_constraints_.extract(constraint_id).mapped();
    RETURN_IF_ERROR(gscip_->DeleteConstraint(gscip_constraint));
    RETURN_IF_ERROR(slack_handler.DeleteStructure(*gscip_));
  }
  RETURN_IF_ERROR(AddSos2Constraints(
      model_update.sos2_constraint_updates().new_constraints()));

  return true;
}

absl::StatusOr<ComputeInfeasibleSubsystemResultProto>
GScipSolver::ComputeInfeasibleSubsystem(const SolveParametersProto&,
                                        MessageCallback,
                                        const SolveInterrupter*) {
  return absl::UnimplementedError(
      "SCIP does not provide a method to compute an infeasible subsystem");
}

MATH_OPT_REGISTER_SOLVER(SOLVER_TYPE_GSCIP, GScipSolver::New)

}  // namespace math_opt
}  // namespace operations_research
