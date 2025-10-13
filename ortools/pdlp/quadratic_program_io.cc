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

#include "ortools/pdlp/quadratic_program_io.h"

#include <cmath>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "Eigen/Core"
#include "Eigen/SparseCore"
#include "absl/base/casts.h"
#include "absl/container/flat_hash_map.h"
#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/match.h"
#include "absl/strings/str_format.h"
#include "absl/strings/string_view.h"
#include "ortools/base/file.h"
#include "ortools/base/helpers.h"
#include "ortools/base/mathutil.h"
#include "ortools/base/options.h"
#include "ortools/base/status_builder.h"
#include "ortools/base/status_macros.h"
#include "ortools/linear_solver/linear_solver.pb.h"
#include "ortools/linear_solver/model_exporter.h"
#include "ortools/lp_data/mps_reader_template.h"
#include "ortools/pdlp/quadratic_program.h"
#include "ortools/util/file_util.h"

namespace operations_research::pdlp {

// TODO(user): Update internal helper functions to use references instead of
// pointers.

QuadraticProgram ReadQuadraticProgramOrDie(const std::string& filename,
                                           bool include_names) {
  if (absl::EndsWith(filename, ".mps") || absl::EndsWith(filename, ".mps.gz") ||
      absl::EndsWith(filename, ".mps.bz2")) {
    return ReadMpsLinearProgramOrDie(filename, include_names);
  }
  if (absl::EndsWith(filename, ".pb") ||
      absl::EndsWith(filename, ".textproto") ||
      absl::EndsWith(filename, ".json") ||
      absl::EndsWith(filename, ".json.gz")) {
    return ReadMPModelProtoFileOrDie(filename, include_names);
  }
  LOG(QFATAL) << "Invalid filename suffix in " << filename
              << ". Valid suffixes are .mps, .mps.gz, .pb, .textproto,"
              << ".json, and .json.gz";
}

QuadraticProgram ReadMPModelProtoFileOrDie(
    const std::string& mpmodel_proto_file, bool include_names) {
  MPModelProto lp_proto;
  QCHECK_OK(ReadFileToProto(mpmodel_proto_file, &lp_proto))
      << mpmodel_proto_file;
  auto result = QpFromMpModelProto(lp_proto, /*relax_integer_variables=*/true,
                                   include_names);
  QCHECK_OK(result);
  return *std::move(result);
}

absl::Status WriteLinearProgramToMps(const QuadraticProgram& linear_program,
                                     const std::string& mps_file) {
  if (!IsLinearProgram(linear_program)) {
    return absl::InvalidArgumentError(
        "'linear_program' has a quadratic objective");
  }
  ASSIGN_OR_RETURN(MPModelProto proto, QpToMpModelProto(linear_program));
  ASSIGN_OR_RETURN(std::string mps_export, ExportModelAsMpsFormat(proto));
  File* file;
  RETURN_IF_ERROR(file::Open(mps_file, "w", &file, file::Defaults()));
  auto status = file::WriteString(file, mps_export, file::Defaults());
  status.Update(file->Close(file::Defaults()));
  return status;
}

absl::Status WriteQuadraticProgramToMPModelProto(
    const QuadraticProgram& quadratic_program,
    const std::string& mpmodel_proto_file) {
  ASSIGN_OR_RETURN(MPModelProto proto, QpToMpModelProto(quadratic_program));

  return file::SetBinaryProto(mpmodel_proto_file, proto, file::Defaults());
}

namespace {

// Class implementing the
// `ortools/lp_data/mps_reader_template.h` interface that only
// stores the names of rows and columns, and the number of non-zeros found.
class MpsReaderDimensionAndNames {
 public:
  using IndexType = int64_t;
  void SetUp() {
    read_or_parse_failed_ = false;
    col_name_to_index_.clear();
    row_name_to_index_.clear();
    added_non_zeros_ = 0;
  }
  void CleanUp() {}
  double ConstraintLowerBound(IndexType index) { return 0; }
  double ConstraintUpperBound(IndexType index) { return 0; }
  IndexType FindOrCreateConstraint(absl::string_view row_name) {
    const auto [it, success_unused] = row_name_to_index_.try_emplace(
        row_name, static_cast<IndexType>(row_name_to_index_.size()));
    return it->second;
  }
  IndexType FindOrCreateVariable(absl::string_view col_name) {
    const auto [it, success_unused] = col_name_to_index_.try_emplace(
        col_name, static_cast<IndexType>(col_name_to_index_.size()));
    return it->second;
  }
  void SetConstraintBounds(IndexType row_index, double lower_bound,
                           double upper_bound) {}
  void SetConstraintCoefficient(IndexType row_index, IndexType col_index,
                                double coefficient) {
    ++added_non_zeros_;
  }
  void SetIsLazy(IndexType row_index) {}
  void SetName(absl::string_view problem_name) {}
  void SetObjectiveCoefficient(IndexType index, double coefficient) {}
  void SetObjectiveDirection(bool maximize) {}
  void SetObjectiveOffset(double offset) {}
  void SetVariableTypeToInteger(IndexType index) {}
  void SetVariableTypeToSemiContinuous(IndexType index) {
    LOG_FIRST_N(WARNING, 1)
        << "Semi-continuous variables not supported, failed to parse file";
    read_or_parse_failed_ = true;
  }
  void SetVariableBounds(IndexType index, double lower_bound,
                         double upper_bound) {}
  double VariableLowerBound(IndexType index) { return 0; }
  double VariableUpperBound(IndexType index) { return 0; }
  absl::Status CreateIndicatorConstraint(absl::string_view row_name,
                                         IndexType col_index, bool var_value) {
    absl::string_view message =
        "Indicator constraints not supported, failed to parse file";
    LOG_FIRST_N(WARNING, 1) << message;
    return absl::InvalidArgumentError(message);
  }

  // Lookup constant functions. They assume that `row_name` or `col_name`,
  // respectively, have been previously added to the internal set of rows or
  // variables. The functions QFAIL if this is not true.
  IndexType FindVariableOrDie(absl::string_view col_name) const {
    const auto it = col_name_to_index_.find(col_name);
    if (it == col_name_to_index_.end()) {
      LOG(QFATAL) << absl::StrFormat(
          "column `%s` not previously found in file, internal error?",
          col_name);
    }
    return it->second;
  }
  IndexType FindConstraintOrDie(absl::string_view row_name) const {
    const auto it = row_name_to_index_.find(row_name);
    if (it == row_name_to_index_.end()) {
      LOG(QFATAL) << absl::StrFormat(
          "row `%s` not previously found in file, internal error?", row_name);
    }
    return it->second;
  }

  // Number of non-zeros added so-far.
  int64_t AddedNonZeros() const { return added_non_zeros_; }

  // Number of variables added so-far.
  int64_t NumVariables() const { return col_name_to_index_.size(); }

  // Number of constraints added so-far.
  int64_t NumConstraints() const { return row_name_to_index_.size(); }

  // Return `true` if an unsupported MPS section was found.
  bool FailedToParse() const { return read_or_parse_failed_; }

  // Return a const-hash map of col names to indices.
  const absl::flat_hash_map<std::string, IndexType>& ColNameIndexMap() const {
    return col_name_to_index_;
  }

  // Return a const-hash map of row names to indices.
  const absl::flat_hash_map<std::string, IndexType>& RowNameIndexMap() const {
    return row_name_to_index_;
  }

 private:
  bool read_or_parse_failed_ = false;
  absl::flat_hash_map<std::string, IndexType> col_name_to_index_;
  absl::flat_hash_map<std::string, IndexType> row_name_to_index_;
  int64_t added_non_zeros_ = 0;
};

// Class implementing the
// `ortools/lp_data/mps_reader_template.h` interface.
// It is intended to be used in conjunction with `MpsReaderDimensionAndNames`
// as follows:
//
// // Retrieve dimension and name information from file.
// MpsReaderDimensionAndNames dimension_and_name;
// MPSReaderTemplate<MpsReaderDimensionAndNames> dimension_parser;
// dimension_parser.ParseFile(file_name, &dimension_and_name);
// // Store QP problem coefficients.
// MpsReaderQpDataWrapper qp_wrapper(&dimension_and_name, include_names);
// MPSReaderTemplate<MpsReaderQpDataWrapper> qp_parser;
// qp_parser.ParseFile(file_name, &qp_wrapper);
// // Retrieve fully assembled QP.
// QuadraticProgram result = qp_wrapper.GetAndClearQuadraticProgram();
class MpsReaderQpDataWrapper {
 public:
  using IndexType = int64_t;
  // Constructor, `*dimension_and_names` must outlive the class, and be
  // constant during the object's lifetime. If `include_names`=true, the
  // resulting QuadraticProgram from `GetAndClearQuadraticProgram()` will
  // include name information from the MPS file.
  // NOTE: The code assumes that the file to be read is the same file already
  // read using the `*dimension_and_names` argument.
  MpsReaderQpDataWrapper(const MpsReaderDimensionAndNames* dimension_and_names,
                         bool include_names)
      : include_names_{include_names},
        dimension_and_names_{*dimension_and_names} {}

  // Implements the `MPSReaderTemplate` interface.
  void SetUp() {
    const int64_t num_variables = dimension_and_names_.NumVariables();
    const int64_t num_constraints = dimension_and_names_.NumConstraints();
    triplets_.reserve(dimension_and_names_.AddedNonZeros());
    quadratic_program_ = QuadraticProgram(/*num_variables=*/num_variables,
                                          /*num_constraints=*/num_constraints);
    // Default variables in MPS files have a zero lower bound, an infinity
    // upper bound, and a zero objective; while default constraints are
    // 'equal to zero' constraints.
    quadratic_program_.constraint_lower_bounds =
        Eigen::VectorXd::Zero(num_constraints);
    quadratic_program_.constraint_upper_bounds =
        Eigen::VectorXd::Zero(num_constraints);
    quadratic_program_.variable_lower_bounds =
        Eigen::VectorXd::Zero(num_variables);
  }
  void CleanUp() {
    SetEigenMatrixFromTriplets(std::move(triplets_),
                               quadratic_program_.constraint_matrix);
    // Deal with maximization problems.
    if (quadratic_program_.objective_scaling_factor == -1) {
      quadratic_program_.objective_offset *= -1;
      for (double& objective_coefficient :
           quadratic_program_.objective_vector) {
        objective_coefficient *= -1;
      }
    }
    if (include_names_) {
      quadratic_program_.variable_names =
          std::vector<std::string>(dimension_and_names_.NumVariables());
      quadratic_program_.constraint_names =
          std::vector<std::string>(dimension_and_names_.NumConstraints());
      for (const auto& [name, index] : dimension_and_names_.ColNameIndexMap()) {
        (*quadratic_program_.variable_names)[index] = name;
      }
      for (const auto& [name, index] : dimension_and_names_.RowNameIndexMap()) {
        (*quadratic_program_.constraint_names)[index] = name;
      }
    }
  }
  double ConstraintLowerBound(IndexType index) {
    return quadratic_program_.constraint_lower_bounds[index];
  }
  double ConstraintUpperBound(IndexType index) {
    return quadratic_program_.constraint_upper_bounds[index];
  }
  IndexType FindOrCreateConstraint(absl::string_view row_name) {
    return dimension_and_names_.FindConstraintOrDie(row_name);
  }
  IndexType FindOrCreateVariable(absl::string_view col_name) {
    return dimension_and_names_.FindVariableOrDie(col_name);
  }
  void SetConstraintBounds(IndexType index, double lower_bound,
                           double upper_bound) {
    quadratic_program_.constraint_lower_bounds[index] = lower_bound;
    quadratic_program_.constraint_upper_bounds[index] = upper_bound;
  }
  void SetConstraintCoefficient(IndexType row_index, IndexType col_index,
                                double coefficient) {
    DCHECK_LT(triplets_.size(), triplets_.capacity());
    triplets_.emplace_back(row_index, col_index, coefficient);
  }
  void SetIsLazy(IndexType row_index) {
    LOG_FIRST_N(WARNING, 1) << "Lazy constraint information lost, treated as "
                               "regular constraint instead";
  }
  void SetName(absl::string_view problem_name) {
    if (include_names_) {
      quadratic_program_.problem_name = std::string(problem_name);
    }
  }
  void SetObjectiveCoefficient(IndexType index, double coefficient) {
    quadratic_program_.objective_vector[index] = coefficient;
  }
  void SetObjectiveDirection(bool maximize) {
    quadratic_program_.objective_scaling_factor = (maximize ? -1 : 1);
  }
  void SetObjectiveOffset(double offset) {
    quadratic_program_.objective_offset = offset;
  }
  void SetVariableTypeToInteger(IndexType index) {
    LOG_FIRST_N(WARNING, 1) << "Dropping integrality requirements, all "
                               "variables treated as continuous";
  }
  void SetVariableTypeToSemiContinuous(IndexType index) {
    // This line should never execute, as the code must fail on
    // `MpsReaderDimensionAndNames::SetVariableTypeToSemiContinuous` before.
    LOG(QFATAL) << "Semi-continuous variables are unsupported, and should have "
                   "been detected before (in MpsReaderDimensionAndNames)";
  }
  void SetVariableBounds(IndexType index, double lower_bound,
                         double upper_bound) {
    quadratic_program_.variable_lower_bounds[index] = lower_bound;
    quadratic_program_.variable_upper_bounds[index] = upper_bound;
  }
  double VariableLowerBound(IndexType index) {
    return quadratic_program_.variable_lower_bounds[index];
  }
  double VariableUpperBound(IndexType index) {
    return quadratic_program_.variable_upper_bounds[index];
  }
  absl::Status CreateIndicatorConstraint(absl::string_view row_name,
                                         IndexType col_index, bool var_value) {
    // This function should never be called, as the code must fail on
    // `MpsReaderDimensionAndNames::CreateIndicatorConstraint` before.
    LOG(QFATAL) << "Indicator constraints are unsupported, and should have "
                   "been detected before (in MpsReaderDimensionAndNames)";
  }

  // Returns a `QuadraticProgram` holding all information read by the
  // `mps_reader_template.h` interface. It leaves the internal quadratic
  // program in an indeterminate state.
  QuadraticProgram&& GetAndClearQuadraticProgram() {
    return std::move(quadratic_program_);
  }

 private:
  bool include_names_;
  QuadraticProgram quadratic_program_;
  const MpsReaderDimensionAndNames& dimension_and_names_;
  std::vector<Eigen::Triplet<double, int64_t>> triplets_;
};

}  // namespace

absl::StatusOr<QuadraticProgram> ReadMpsLinearProgram(
    const std::string& lp_file, bool include_names) {
  MpsReaderDimensionAndNames dimension_and_names;
  // Collect MPS format, sizes and names.
  MPSReaderTemplate<MpsReaderDimensionAndNames> pass_one_reader;
  const absl::StatusOr<MPSReaderFormat> pass_one_format =
      pass_one_reader.ParseFile(lp_file, &dimension_and_names,
                                MPSReaderFormat::kAutoDetect);
  if (!pass_one_format.ok()) {
    return util::StatusBuilder(pass_one_format.status()) << absl::StrFormat(
               "Could not read or parse file `%s` as an MPS file", lp_file);
  }
  if (dimension_and_names.FailedToParse()) {
    return absl::InvalidArgumentError(absl::StrFormat(
        "Could not read or parse file `%s` as an MPS file, or unsupported "
        "features/sections found",
        lp_file));
  }
  DCHECK(*pass_one_format == MPSReaderFormat::kFixed ||
         *pass_one_format == MPSReaderFormat::kFree);
  // Populate the QP with pre-allocated sizes.
  MpsReaderQpDataWrapper qp_data_wrapper(&dimension_and_names, include_names);
  MPSReaderTemplate<MpsReaderQpDataWrapper> pass_two_reader;
  const absl::StatusOr<MPSReaderFormat> pass_two_format =
      pass_two_reader.ParseFile(lp_file, &qp_data_wrapper, *pass_one_format);
  if (!pass_two_format.ok()) {
    return util::StatusBuilder(pass_two_format.status()) << absl::StrFormat(
               "Could not read or parse file `%s` as an MPS file "
               "(maybe file changed between reads?)",
               lp_file);
  }
  DCHECK(*pass_one_format == *pass_two_format);
  return qp_data_wrapper.GetAndClearQuadraticProgram();
}

QuadraticProgram ReadMpsLinearProgramOrDie(const std::string& lp_file,
                                           bool include_names) {
  const absl::StatusOr<QuadraticProgram> result =
      ReadMpsLinearProgram(lp_file, include_names);
  if (!result.ok()) {
    LOG(QFATAL) << "Error reading MPS Linear Program from " << lp_file << ": "
                << result.status().message();
  }
  return *result;
}

}  // namespace operations_research::pdlp
