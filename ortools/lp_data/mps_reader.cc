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

#include "ortools/lp_data/mps_reader.h"

#include <limits>
#include <string>
#include <vector>

#include "absl/container/btree_set.h"
#include "absl/container/flat_hash_map.h"
#include "absl/log/check.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "ortools/base/logging.h"
#include "ortools/base/protobuf_util.h"
#include "ortools/base/status_macros.h"
#include "ortools/linear_solver/linear_solver.pb.h"
#include "ortools/lp_data/lp_data.h"
#include "ortools/lp_data/lp_types.h"
#include "ortools/lp_data/mps_reader_template.h"

namespace operations_research {
namespace glop {

// Data templates.

template <class Data>
class DataWrapper {};

template <>
class DataWrapper<LinearProgram> {
 public:
  using IndexType = int;
  explicit DataWrapper(LinearProgram* data) { data_ = data; }

  void SetUp() {
    data_->SetDcheckBounds(false);
    data_->Clear();
  }

  void SetName(absl::string_view name) { data_->SetName(name); }

  void SetObjectiveDirection(bool maximize) {
    data_->SetMaximizationProblem(maximize);
  }

  void SetObjectiveOffset(double objective_offset) {
    data_->SetObjectiveOffset(objective_offset);
  }

  int FindOrCreateConstraint(absl::string_view name) {
    return data_->FindOrCreateConstraint(name).value();
  }
  void SetConstraintBounds(int index, double lower_bound, double upper_bound) {
    data_->SetConstraintBounds(RowIndex(index), lower_bound, upper_bound);
  }
  void SetConstraintCoefficient(int row_index, int col_index,
                                double coefficient) {
    data_->SetCoefficient(RowIndex(row_index), ColIndex(col_index),
                          coefficient);
  }
  void SetIsLazy(int row_index) {
    LOG_FIRST_N(WARNING, 1)
        << "LAZYCONS section detected. It will be handled as an extension of "
           "the ROWS section.";
  }
  double ConstraintLowerBound(int row_index) {
    return data_->constraint_lower_bounds()[RowIndex(row_index)];
  }
  double ConstraintUpperBound(int row_index) {
    return data_->constraint_upper_bounds()[RowIndex(row_index)];
  }

  int FindOrCreateVariable(absl::string_view name) {
    return data_->FindOrCreateVariable(name).value();
  }
  void SetVariableTypeToInteger(int index) {
    data_->SetVariableType(ColIndex(index),
                           LinearProgram::VariableType::INTEGER);
  }
  void SetVariableTypeToSemiContinuous(int index) {
    LOG(FATAL) << "Semi continuous variables are not supported";
  }
  void SetVariableBounds(int index, double lower_bound, double upper_bound) {
    data_->SetVariableBounds(ColIndex(index), lower_bound, upper_bound);
  }
  void SetObjectiveCoefficient(int index, double coefficient) {
    data_->SetObjectiveCoefficient(ColIndex(index), coefficient);
  }
  double VariableLowerBound(int index) {
    return data_->variable_lower_bounds()[ColIndex(index)];
  }
  double VariableUpperBound(int index) {
    return data_->variable_upper_bounds()[ColIndex(index)];
  }

  absl::Status CreateIndicatorConstraint(absl::string_view row_name,
                                         int col_index, bool col_value) {
    return absl::UnimplementedError(
        "LinearProgram does not support indicator constraints.");
  }

  void CleanUp() { data_->CleanUp(); }

 private:
  LinearProgram* data_;
};

template <>
class DataWrapper<MPModelProto> {
 public:
  using IndexType = int;
  explicit DataWrapper(MPModelProto* data) { data_ = data; }

  void SetUp() {
    data_->Clear();
    variable_indices_by_name_.clear();
    constraint_indices_by_name_.clear();
    constraints_to_delete_.clear();
    semi_continuous_variables_.clear();
  }

  void SetName(absl::string_view name) { data_->set_name(name); }

  void SetObjectiveDirection(bool maximize) { data_->set_maximize(maximize); }

  void SetObjectiveOffset(double objective_offset) {
    data_->set_objective_offset(objective_offset);
  }

  int FindOrCreateConstraint(absl::string_view name) {
    const auto it = constraint_indices_by_name_.find(name);
    if (it != constraint_indices_by_name_.end()) return it->second;

    const int index = data_->constraint_size();
    MPConstraintProto* const constraint = data_->add_constraint();
    constraint->set_lower_bound(0.0);
    constraint->set_upper_bound(0.0);
    constraint->set_name(name);
    constraint_indices_by_name_[name] = index;
    return index;
  }
  void SetConstraintBounds(int index, double lower_bound, double upper_bound) {
    data_->mutable_constraint(index)->set_lower_bound(lower_bound);
    data_->mutable_constraint(index)->set_upper_bound(upper_bound);
  }
  void SetConstraintCoefficient(int row_index, int col_index,
                                double coefficient) {
    // Note that we assume that there is no duplicate in the mps file format. If
    // there is, we will just add more than one entry from the same variable in
    // a constraint, and we let any program that ingests an MPModelProto handle
    // it.
    MPConstraintProto* const constraint = data_->mutable_constraint(row_index);
    constraint->add_var_index(col_index);
    constraint->add_coefficient(coefficient);
  }
  void SetIsLazy(int row_index) {
    data_->mutable_constraint(row_index)->set_is_lazy(true);
  }
  double ConstraintLowerBound(int row_index) {
    return data_->constraint(row_index).lower_bound();
  }
  double ConstraintUpperBound(int row_index) {
    return data_->constraint(row_index).upper_bound();
  }

  int FindOrCreateVariable(absl::string_view name) {
    const auto it = variable_indices_by_name_.find(name);
    if (it != variable_indices_by_name_.end()) return it->second;

    const int index = data_->variable_size();
    MPVariableProto* const variable = data_->add_variable();
    variable->set_lower_bound(0.0);
    variable->set_name(name);
    variable_indices_by_name_[name] = index;
    return index;
  }
  void SetVariableTypeToInteger(int index) {
    data_->mutable_variable(index)->set_is_integer(true);
  }
  void SetVariableTypeToSemiContinuous(int index) {
    semi_continuous_variables_.push_back(index);
  }
  void SetVariableBounds(int index, double lower_bound, double upper_bound) {
    data_->mutable_variable(index)->set_lower_bound(lower_bound);
    data_->mutable_variable(index)->set_upper_bound(upper_bound);
  }
  void SetObjectiveCoefficient(int index, double coefficient) {
    data_->mutable_variable(index)->set_objective_coefficient(coefficient);
  }
  double VariableLowerBound(int index) {
    return data_->variable(index).lower_bound();
  }
  double VariableUpperBound(int index) {
    return data_->variable(index).upper_bound();
  }

  absl::Status CreateIndicatorConstraint(absl::string_view cst_name,
                                         int var_index, bool var_value) {
    const auto it = constraint_indices_by_name_.find(cst_name);
    if (it == constraint_indices_by_name_.end()) {
      return absl::InvalidArgumentError(
          absl::StrCat("Constraint \"", cst_name, "\" doesn't exist."));
    }
    const int cst_index = it->second;

    MPGeneralConstraintProto* const constraint =
        data_->add_general_constraint();
    constraint->set_name(
        absl::StrCat("ind_", data_->constraint(cst_index).name()));
    MPIndicatorConstraint* const indicator =
        constraint->mutable_indicator_constraint();
    *indicator->mutable_constraint() = data_->constraint(cst_index);
    indicator->set_var_index(var_index);
    indicator->set_var_value(var_value);
    constraints_to_delete_.insert(cst_index);

    return absl::OkStatus();
  }

  void CleanUp() {
    google::protobuf::util::RemoveAt(data_->mutable_constraint(),
                                     constraints_to_delete_);

    for (const int index : semi_continuous_variables_) {
      MPVariableProto* mp_var = data_->mutable_variable(index);
      // We detect that the lower bound was not set when it is left to its
      // default value of zero.
      const double lb =
          mp_var->lower_bound() == 0 ? 1.0 : mp_var->lower_bound();
      DCHECK_GT(lb, 0.0);
      const double ub = mp_var->upper_bound();
      mp_var->set_lower_bound(0.0);

      // Create a new Boolean variable.
      const int bool_var_index = data_->variable_size();
      MPVariableProto* bool_var = data_->add_variable();
      bool_var->set_lower_bound(0.0);
      bool_var->set_upper_bound(1.0);
      bool_var->set_is_integer(true);

      // TODO(user): Experiment with the switch constant.
      if (ub >= 1e8) {  // Use indicator constraints
        // bool_var == 0 implies var == 0.
        MPGeneralConstraintProto* const zero_constraint =
            data_->add_general_constraint();
        MPIndicatorConstraint* const zero_indicator =
            zero_constraint->mutable_indicator_constraint();
        zero_indicator->set_var_index(bool_var_index);
        zero_indicator->set_var_value(0);
        zero_indicator->mutable_constraint()->set_lower_bound(0.0);
        zero_indicator->mutable_constraint()->set_upper_bound(0.0);
        zero_indicator->mutable_constraint()->add_var_index(index);
        zero_indicator->mutable_constraint()->add_coefficient(1.0);

        // bool_var == 1 implies lb <= var <= ub
        MPGeneralConstraintProto* const one_constraint =
            data_->add_general_constraint();
        MPIndicatorConstraint* const one_indicator =
            one_constraint->mutable_indicator_constraint();
        one_indicator->set_var_index(bool_var_index);
        one_indicator->set_var_value(1);
        one_indicator->mutable_constraint()->set_lower_bound(lb);
        one_indicator->mutable_constraint()->set_upper_bound(ub);
        one_indicator->mutable_constraint()->add_var_index(index);
        one_indicator->mutable_constraint()->add_coefficient(1.0);
      } else {  // Pure linear encoding.
        // var >= bool_var * lb
        MPConstraintProto* lower = data_->add_constraint();
        lower->set_lower_bound(0.0);
        lower->set_upper_bound(std::numeric_limits<double>::infinity());
        lower->add_var_index(index);
        lower->add_coefficient(1.0);
        lower->add_var_index(bool_var_index);
        lower->add_coefficient(-lb);

        // var <= bool_var * ub
        MPConstraintProto* upper = data_->add_constraint();
        upper->set_lower_bound(-std::numeric_limits<double>::infinity());
        upper->set_upper_bound(0.0);
        upper->add_var_index(index);
        upper->add_coefficient(1.0);
        upper->add_var_index(bool_var_index);
        upper->add_coefficient(-ub);
      }
    }
  }

 private:
  MPModelProto* data_;

  absl::flat_hash_map<std::string, int> variable_indices_by_name_;
  absl::flat_hash_map<std::string, int> constraint_indices_by_name_;
  absl::btree_set<int> constraints_to_delete_;
  std::vector<int> semi_continuous_variables_;
};

namespace {

// Translates MPSReader::Form into MPSReaderFormat, with `kAutoDetect` as
// default value (even for invalid values of `form`).
MPSReaderFormat TemplateFormat(MPSReader::Form form) {
  return (form == MPSReader::FIXED)  ? MPSReaderFormat::kFixed
         : (form == MPSReader::FREE) ? MPSReaderFormat::kFree
                                     : MPSReaderFormat::kAutoDetect;
}

}  // namespace

// Parses instance from a file.
absl::Status MPSReader::ParseFile(absl::string_view file_name,
                                  LinearProgram* data, Form form) {
  DataWrapper<LinearProgram> data_wrapper(data);
  return MPSReaderTemplate<DataWrapper<LinearProgram>>()
      .ParseFile(file_name, &data_wrapper, TemplateFormat(form))
      .status();
}

absl::Status MPSReader::ParseFile(absl::string_view file_name,
                                  MPModelProto* data, Form form) {
  DataWrapper<MPModelProto> data_wrapper(data);
  return MPSReaderTemplate<DataWrapper<MPModelProto>>()
      .ParseFile(file_name, &data_wrapper, TemplateFormat(form))
      .status();
}

// Loads instance from string. Useful with MapReduce. Automatically detects
// the file's format (free or fixed).
absl::Status MPSReader::ParseProblemFromString(absl::string_view source,
                                               LinearProgram* data,
                                               MPSReader::Form form) {
  DataWrapper<LinearProgram> data_wrapper(data);
  return MPSReaderTemplate<DataWrapper<LinearProgram>>()
      .ParseString(source, &data_wrapper, TemplateFormat(form))
      .status();
}

absl::Status MPSReader::ParseProblemFromString(absl::string_view source,
                                               MPModelProto* data,
                                               MPSReader::Form form) {
  DataWrapper<MPModelProto> data_wrapper(data);
  return MPSReaderTemplate<DataWrapper<MPModelProto>>()
      .ParseString(source, &data_wrapper, TemplateFormat(form))
      .status();
}

absl::StatusOr<MPModelProto> MpsDataToMPModelProto(absl::string_view mps_data) {
  MPModelProto model;
  DataWrapper<MPModelProto> data_wrapper(&model);
  RETURN_IF_ERROR(
      (MPSReaderTemplate<DataWrapper<MPModelProto>>()
           .ParseString(mps_data, &data_wrapper, MPSReaderFormat::kAutoDetect)
           .status()));
  return model;
}

absl::StatusOr<MPModelProto> MpsFileToMPModelProto(absl::string_view mps_file) {
  MPModelProto model;
  DataWrapper<MPModelProto> data_wrapper(&model);
  RETURN_IF_ERROR(
      (MPSReaderTemplate<DataWrapper<MPModelProto>>()
           .ParseFile(mps_file, &data_wrapper, MPSReaderFormat::kAutoDetect)
           .status()));
  return model;
}

}  // namespace glop
}  // namespace operations_research
