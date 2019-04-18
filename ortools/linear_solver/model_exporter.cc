// Copyright 2010-2018 Google LLC
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

#include "ortools/linear_solver/model_exporter.h"

#include <algorithm>
#include <cmath>
#include <limits>

#include "absl/container/flat_hash_set.h"
#include "absl/strings/ascii.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "ortools/base/canonical_errors.h"
#include "ortools/base/commandlineflags.h"
#include "ortools/base/integral_types.h"
#include "ortools/base/logging.h"
#include "ortools/base/map_util.h"
#include "ortools/linear_solver/linear_solver.pb.h"
#include "ortools/util/fp_utils.h"

DEFINE_bool(lp_log_invalid_name, false, "DEPRECATED.");

namespace operations_research {
namespace {

constexpr double kInfinity = std::numeric_limits<double>::infinity();

class MPModelProtoExporter {
 public:
  explicit MPModelProtoExporter(const MPModelProto& model);
  bool ExportModelAsLpFormat(const MPModelExportOptions& options,
                             std::string* output);
  bool ExportModelAsMpsFormat(const MPModelExportOptions& options,
                              std::string* output);

 private:
  // Computes the number of continuous, integer and binary variables.
  // Called by ExportModelAsLpFormat() and ExportModelAsMpsFormat().
  void Setup();

  // Computes smart column widths for free MPS format.
  void ComputeMpsSmartColumnWidths(bool obfuscated);

  // Processes all the proto.name() fields and returns the result in a vector.
  //
  // If 'obfuscate' is true, none of names are actually used, and this just
  // returns a vector of 'prefix' + proto index (1-based).
  //
  // If it is false, this tries to keep the original names, but:
  // - if the first character is forbidden, '_' is added at the beginning of
  //   name.
  // - all the other forbidden characters are replaced by '_'.
  // To avoid name conflicts, a '_' followed by an integer is appended to the
  // result.
  //
  // If a name is longer than the maximum allowed name length, the obfuscated
  // name is used.
  //
  // Therefore, a name "$20<=40" for proto #3 could be "_$20__40_1".
  template <class ListOfProtosWithNameFields>
  std::vector<std::string> ExtractAndProcessNames(
      const ListOfProtosWithNameFields& proto, const std::string& prefix,
      bool obfuscate, bool log_invalid_names,
      const std::string& forbidden_first_chars,
      const std::string& forbidden_chars);

  // Appends a general "Comment" section with useful metadata about the model
  // to "output".
  // Note(user): there may be less variables in output than in the original
  // model, as unused variables are not shown by default. Similarly, there
  // may be more constraints in a .lp file as in the original model as
  // a constraint lhs <= term <= rhs will be output as the two constraints
  // term >= lhs and term <= rhs.
  void AppendComments(const std::string& separator, std::string* output) const;

  // Clears "output" and writes a term to it, in "Lp" format. Returns false on
  // error (for example, var_index is out of range).
  bool WriteLpTerm(int var_index, double coefficient,
                   std::string* output) const;

  // Appends a pair name, value to "output", formatted to comply with the MPS
  // standard.
  void AppendMpsPair(const std::string& name, double value,
                     std::string* output) const;

  // Appends the head of a line, consisting of an id and a name to output.
  void AppendMpsLineHeader(const std::string& id, const std::string& name,
                           std::string* output) const;

  // Same as AppendMpsLineHeader. Appends an extra new-line at the end the
  // std::string pointed to by output.
  void AppendMpsLineHeaderWithNewLine(const std::string& id,
                                      const std::string& name,
                                      std::string* output) const;

  // Appends an MPS term in various contexts. The term consists of a head name,
  // a name, and a value. If the line is not empty, then only the pair
  // (name, value) is appended. The number of columns, limited to 2 by the MPS
  // format is also taken care of.
  void AppendMpsTermWithContext(const std::string& head_name,
                                const std::string& name, double value,
                                std::string* output);

  // Appends a new-line if two columns are already present on the MPS line.
  // Used by and in complement to AppendMpsTermWithContext.
  void AppendNewLineIfTwoColumns(std::string* output);

  // When 'integrality' is true, appends columns corresponding to integer
  // variables. Appends the columns for non-integer variables otherwise.
  // The sparse matrix must be passed as a vector of columns ('transpose').
  void AppendMpsColumns(
      bool integrality,
      const std::vector<std::vector<std::pair<int, double>>>& transpose,
      std::string* output);

  // Appends a line describing the bound of a variablenew-line if two columns
  // are already present on the MPS line.
  // Used by and in complement to AppendMpsTermWithContext.
  void AppendMpsBound(const std::string& bound_type, const std::string& name,
                      double value, std::string* output) const;

  const MPModelProto& proto_;

  // Vector of variable names as they will be exported.
  std::vector<std::string> exported_variable_names_;

  // Vector of constraint names as they will be exported.
  std::vector<std::string> exported_constraint_names_;

  // Number of integer variables in proto_.
  int num_integer_variables_;

  // Number of binary variables in proto_.
  int num_binary_variables_;

  // Number of continuous variables in proto_.
  int num_continuous_variables_;

  // Current MPS file column number.
  int current_mps_column_;

  // Format for MPS file lines.
  std::unique_ptr<absl::ParsedFormat<'s', 's'>> mps_header_format_;
  std::unique_ptr<absl::ParsedFormat<'s', 's'>> mps_format_;

  DISALLOW_COPY_AND_ASSIGN(MPModelProtoExporter);
};
}  // namespace

util::StatusOr<std::string> ExportModelAsLpFormat(
    const MPModelProto& model, const MPModelExportOptions& options) {
  MPModelProtoExporter exporter(model);
  std::string output;
  if (!exporter.ExportModelAsLpFormat(options, &output)) {
    return util::InvalidArgumentError("Unable to export model.");
  }
  return output;
}

util::StatusOr<std::string> ExportModelAsMpsFormat(
    const MPModelProto& model, const MPModelExportOptions& options) {
  MPModelProtoExporter exporter(model);
  std::string output;
  if (!exporter.ExportModelAsMpsFormat(options, &output)) {
    return util::InvalidArgumentError("Unable to export model.");
  }
  return output;
}

namespace {
MPModelProtoExporter::MPModelProtoExporter(const MPModelProto& model)
    : proto_(model),
      num_integer_variables_(0),
      num_binary_variables_(0),
      num_continuous_variables_(0),
      current_mps_column_(0) {}

namespace {
class NameManager {
 public:
  NameManager() : names_set_(), last_n_(1) {}
  std::string MakeUniqueName(const std::string& name);

 private:
  absl::flat_hash_set<std::string> names_set_;
  int last_n_;
};

std::string NameManager::MakeUniqueName(const std::string& name) {
  std::string result = name;
  // Find the 'n' so that "name_n" does not already exist.
  int n = last_n_;
  while (!names_set_.insert(result).second) {
    result = absl::StrCat(name, "_", n);
    ++n;
  }
  // We keep the last n used to avoid a quadratic behavior in case
  // all the names are the same initially.
  last_n_ = n;
  return result;
}

std::string MakeExportableName(const std::string& name,
                               const std::string& forbidden_first_chars,
                               const std::string& forbidden_chars,
                               bool* found_forbidden_char) {
  // Prepend with "_" all the names starting with a forbidden character.
  *found_forbidden_char =
      forbidden_first_chars.find(name[0]) != std::string::npos;
  std::string exportable_name =
      *found_forbidden_char ? absl::StrCat("_", name) : name;

  // Replace all the other forbidden characters with "_".
  for (char& c : exportable_name) {
    if (forbidden_chars.find(c) != std::string::npos) {
      c = '_';
      *found_forbidden_char = true;
    }
  }
  return exportable_name;
}
}  // namespace

template <class ListOfProtosWithNameFields>
std::vector<std::string> MPModelProtoExporter::ExtractAndProcessNames(
    const ListOfProtosWithNameFields& proto, const std::string& prefix,
    bool obfuscate, bool log_invalid_names,
    const std::string& forbidden_first_chars,
    const std::string& forbidden_chars) {
  const int num_items = proto.size();
  std::vector<std::string> result(num_items);
  NameManager namer;
  const int num_digits = absl::StrCat(num_items).size();
  int i = 0;
  for (const auto& item : proto) {
    const std::string obfuscated_name =
        absl::StrFormat("%s%0*d", prefix, num_digits, i);
    if (obfuscate || !item.has_name()) {
      result[i] = namer.MakeUniqueName(obfuscated_name);
      LOG_IF(WARNING, log_invalid_names && !item.has_name())
          << "Empty name detected, created new name: " << result[i];
    } else {
      bool found_forbidden_char = false;
      const std::string exportable_name =
          MakeExportableName(item.name(), forbidden_first_chars,
                             forbidden_chars, &found_forbidden_char);
      result[i] = namer.MakeUniqueName(exportable_name);
      LOG_IF(WARNING, log_invalid_names && found_forbidden_char)
          << "Invalid character detected in " << item.name() << ". Changed to "
          << result[i];
      // If the name is too long, use the obfuscated name that is guaranteed
      // to fit. If ever we are able to solve problems with 2^64 variables,
      // their obfuscated names would fit within 20 characters.
      const int kMaxNameLength = 255;
      // Take care of "_rhs" or "_lhs" that may be added in the case of
      // constraints with both right-hand side and left-hand side.
      const int kMargin = 4;
      if (result[i].size() > kMaxNameLength - kMargin) {
        const std::string old_name = std::move(result[i]);
        result[i] = namer.MakeUniqueName(obfuscated_name);
        LOG_IF(WARNING, log_invalid_names) << "Name is too long: " << old_name
                                           << " exported as: " << result[i];
      }
    }

    // Prepare for the next round.
    ++i;
  }
  return result;
}

void MPModelProtoExporter::AppendComments(const std::string& separator,
                                          std::string* output) const {
  const char* const sep = separator.c_str();
  absl::StrAppendFormat(output, "%s Generated by MPModelProtoExporter\n", sep);
  absl::StrAppendFormat(output, "%s   %-16s : %s\n", sep, "Name",
                        proto_.has_name() ? proto_.name().c_str() : "NoName");
  absl::StrAppendFormat(output, "%s   %-16s : %s\n", sep, "Format", "Free");
  absl::StrAppendFormat(output, "%s   %-16s : %d\n", sep, "Constraints",
                        proto_.constraint_size());
  absl::StrAppendFormat(output, "%s   %-16s : %d\n", sep, "Variables",
                        proto_.variable_size());
  absl::StrAppendFormat(output, "%s     %-14s : %d\n", sep, "Binary",
                        num_binary_variables_);
  absl::StrAppendFormat(output, "%s     %-14s : %d\n", sep, "Integer",
                        num_integer_variables_);
  absl::StrAppendFormat(output, "%s     %-14s : %d\n", sep, "Continuous",
                        num_continuous_variables_);
}

namespace {
class LineBreaker {
 public:
  explicit LineBreaker(int max_line_size)
      : max_line_size_(max_line_size), line_size_(0), output_() {}
  // Lines are broken in such a way that:
  // - Strings that are given to Append() are never split.
  // - Lines are split so that their length doesn't exceed the max length;
  //   unless a single std::string given to Append() exceeds that length (in
  //   which case it will be put alone on a single unsplit line).
  void Append(const std::string& s);

  // Returns true if std::string s will fit on the current line without adding
  // a carriage return.
  bool WillFit(const std::string& s) {
    return line_size_ + s.size() < max_line_size_;
  }

  // "Consumes" size characters on the line. Used when starting the constraint
  // lines.
  void Consume(int size) { line_size_ += size; }

  std::string GetOutput() const { return output_; }

 private:
  int max_line_size_;
  int line_size_;
  std::string output_;
};

void LineBreaker::Append(const std::string& s) {
  line_size_ += s.size();
  if (line_size_ > max_line_size_) {
    line_size_ = s.size();
    absl::StrAppend(&output_, "\n ");
  }
  absl::StrAppend(&output_, s);
}

std::string DoubleToStringWithForcedSign(double d) {
  return absl::StrCat((d < 0 ? "" : "+"), (d));
}

std::string DoubleToString(double d) { return absl::StrCat((d)); }

}  // namespace

bool MPModelProtoExporter::WriteLpTerm(int var_index, double coefficient,
                                       std::string* output) const {
  output->clear();
  if (var_index < 0 || var_index >= proto_.variable_size()) {
    LOG(DFATAL) << "Reference to out-of-bounds variable index # " << var_index;
    return false;
  }
  if (coefficient != 0.0) {
    *output = absl::StrCat(DoubleToStringWithForcedSign(coefficient), " ",
                           exported_variable_names_[var_index], " ");
  }
  return true;
}

namespace {
bool IsBoolean(const MPVariableProto& var) {
  return var.is_integer() && ceil(var.lower_bound()) == 0.0 &&
         floor(var.upper_bound()) == 1.0;
}

void UpdateMaxSize(const std::string& new_string, int* size) {
  if (new_string.size() > *size) *size = new_string.size();
}

void UpdateMaxSize(double new_number, int* size) {
  UpdateMaxSize(DoubleToString(new_number), size);
}
}  // namespace

void MPModelProtoExporter::Setup() {
  if (FLAGS_lp_log_invalid_name) {
    LOG(WARNING) << "The \"lp_log_invalid_name\" flag is deprecated. Use "
                    "MPModelProtoExportOptions instead.";
  }
  num_binary_variables_ = 0;
  num_integer_variables_ = 0;
  for (const MPVariableProto& var : proto_.variable()) {
    if (var.is_integer()) {
      if (IsBoolean(var)) {
        ++num_binary_variables_;
      } else {
        ++num_integer_variables_;
      }
    }
  }
  num_continuous_variables_ =
      proto_.variable_size() - num_binary_variables_ - num_integer_variables_;
}

void MPModelProtoExporter::ComputeMpsSmartColumnWidths(bool obfuscated) {
  // Minimum values for aesthetics (if columns are too narrow, MPS files are
  // difficult to read).
  int string_field_size = 6;
  int number_field_size = 6;

  for (const MPVariableProto& var : proto_.variable()) {
    UpdateMaxSize(var.name(), &string_field_size);
    UpdateMaxSize(var.objective_coefficient(), &number_field_size);
    UpdateMaxSize(var.lower_bound(), &number_field_size);
    UpdateMaxSize(var.upper_bound(), &number_field_size);
  }

  for (const MPConstraintProto& cst : proto_.constraint()) {
    UpdateMaxSize(cst.name(), &string_field_size);
    UpdateMaxSize(cst.lower_bound(), &number_field_size);
    UpdateMaxSize(cst.upper_bound(), &number_field_size);
    for (const double coeff : cst.coefficient()) {
      UpdateMaxSize(coeff, &number_field_size);
    }
  }

  // Maximum values for aesthetics. These are also the values used by other
  // solvers.
  string_field_size = std::min(string_field_size, 255);
  number_field_size = std::min(number_field_size, 255);

  // If the model is obfuscated, all names will have the same size, which we
  // compute here.
  if (obfuscated) {
    int max_digits =
        absl::StrCat(
            std::max(proto_.variable_size(), proto_.constraint_size()) - 1)
            .size();
    string_field_size = std::max(6, max_digits + 1);
  }

  mps_header_format_ = absl::ParsedFormat<'s', 's'>::New(
      absl::StrCat(" %-2s %-", string_field_size, "s"));
  mps_format_ = absl::ParsedFormat<'s', 's'>::New(
      absl::StrCat("  %-", string_field_size, "s  %", number_field_size, "s"));
}

bool MPModelProtoExporter::ExportModelAsLpFormat(
    const MPModelExportOptions& options, std::string* output) {
  output->clear();
  Setup();
  const std::string kForbiddenFirstChars = "$.0123456789";
  const std::string kForbiddenChars = " +-*/<>=:\\";
  exported_constraint_names_ = ExtractAndProcessNames(
      proto_.constraint(), "C", options.obfuscate, options.log_invalid_names,
      kForbiddenFirstChars, kForbiddenChars);
  exported_variable_names_ = ExtractAndProcessNames(
      proto_.variable(), "V", options.obfuscate, options.log_invalid_names,
      kForbiddenFirstChars, kForbiddenChars);

  // Comments section.
  AppendComments("\\", output);
  if (options.show_unused_variables) {
    absl::StrAppendFormat(output, "\\ Unused variables are shown\n");
  }

  // Objective
  absl::StrAppend(output, proto_.maximize() ? "Maximize\n" : "Minimize\n");
  LineBreaker obj_line_breaker(options.max_line_length);
  obj_line_breaker.Append(" Obj: ");
  if (proto_.objective_offset() != 0.0) {
    obj_line_breaker.Append(absl::StrCat(
        DoubleToStringWithForcedSign(proto_.objective_offset()), " Constant "));
  }
  std::vector<bool> show_variable(proto_.variable_size(),
                                  options.show_unused_variables);
  for (int var_index = 0; var_index < proto_.variable_size(); ++var_index) {
    const double coeff = proto_.variable(var_index).objective_coefficient();
    std::string term;
    if (!WriteLpTerm(var_index, coeff, &term)) {
      return false;
    }
    obj_line_breaker.Append(term);
    show_variable[var_index] = coeff != 0.0 || options.show_unused_variables;
  }
  // Constraints
  absl::StrAppend(output, obj_line_breaker.GetOutput(), "\nSubject to\n");
  for (int cst_index = 0; cst_index < proto_.constraint_size(); ++cst_index) {
    const MPConstraintProto& ct_proto = proto_.constraint(cst_index);
    const std::string& name = exported_constraint_names_[cst_index];
    LineBreaker line_breaker(options.max_line_length);
    const int kNumFormattingChars = 10;  // Overevaluated.
    // Account for the size of the constraint name + possibly "_rhs" +
    // the formatting characters here.
    line_breaker.Consume(kNumFormattingChars + name.size());
    for (int i = 0; i < ct_proto.var_index_size(); ++i) {
      const int var_index = ct_proto.var_index(i);
      const double coeff = ct_proto.coefficient(i);
      std::string term;
      if (!WriteLpTerm(var_index, coeff, &term)) {
        return false;
      }
      line_breaker.Append(term);
      show_variable[var_index] = coeff != 0.0 || options.show_unused_variables;
    }
    const double lb = ct_proto.lower_bound();
    const double ub = ct_proto.upper_bound();
    if (lb == ub) {
      line_breaker.Append(absl::StrCat(" = ", DoubleToString(ub), "\n"));
      absl::StrAppend(output, " ", name, ": ", line_breaker.GetOutput());
    } else {
      if (ub != +kInfinity) {
        std::string rhs_name = name;
        if (lb != -kInfinity) {
          absl::StrAppend(&rhs_name, "_rhs");
        }
        absl::StrAppend(output, " ", rhs_name, ": ", line_breaker.GetOutput());
        const std::string relation =
            absl::StrCat(" <= ", DoubleToString(ub), "\n");
        // Here we have to make sure we do not add the relation to the contents
        // of line_breaker, which may be used in the subsequent clause.
        if (!line_breaker.WillFit(relation)) absl::StrAppend(output, "\n ");
        absl::StrAppend(output, relation);
      }
      if (lb != -kInfinity) {
        std::string lhs_name = name;
        if (ub != +kInfinity) {
          absl::StrAppend(&lhs_name, "_lhs");
        }
        absl::StrAppend(output, " ", lhs_name, ": ", line_breaker.GetOutput());
        const std::string relation =
            absl::StrCat(" >= ", DoubleToString(lb), "\n");
        if (!line_breaker.WillFit(relation)) absl::StrAppend(output, "\n ");
        absl::StrAppend(output, relation);
      }
    }
  }

  // Bounds
  absl::StrAppend(output, "Bounds\n");
  if (proto_.objective_offset() != 0.0) {
    absl::StrAppend(output, " 1 <= Constant <= 1\n");
  }
  for (int var_index = 0; var_index < proto_.variable_size(); ++var_index) {
    if (!show_variable[var_index]) continue;
    const MPVariableProto& var_proto = proto_.variable(var_index);
    const double lb = var_proto.lower_bound();
    const double ub = var_proto.upper_bound();
    if (var_proto.is_integer() && lb == round(lb) && ub == round(ub)) {
      absl::StrAppendFormat(output, " %.0f <= %s <= %.0f\n", lb,
                            exported_variable_names_[var_index], ub);
    } else {
      absl::StrAppend(output, " ");
      if (lb == -kInfinity && ub == kInfinity) {
        absl::StrAppend(output, exported_variable_names_[var_index], " free");
      } else {
        if (lb != -kInfinity) {
          absl::StrAppend(output, DoubleToString(lb), " <= ");
        }
        absl::StrAppend(output, exported_variable_names_[var_index]);
        if (ub != kInfinity) {
          absl::StrAppend(output, " <= ", DoubleToString(ub));
        }
      }
      absl::StrAppend(output, "\n");
    }
  }

  // Binaries
  if (num_binary_variables_ > 0) {
    absl::StrAppend(output, "Binaries\n");
    for (int var_index = 0; var_index < proto_.variable_size(); ++var_index) {
      if (!show_variable[var_index]) continue;
      const MPVariableProto& var_proto = proto_.variable(var_index);
      if (IsBoolean(var_proto)) {
        absl::StrAppendFormat(output, " %s\n",
                              exported_variable_names_[var_index]);
      }
    }
  }

  // Generals
  if (num_integer_variables_ > 0) {
    absl::StrAppend(output, "Generals\n");
    for (int var_index = 0; var_index < proto_.variable_size(); ++var_index) {
      if (!show_variable[var_index]) continue;
      const MPVariableProto& var_proto = proto_.variable(var_index);
      if (var_proto.is_integer() && !IsBoolean(var_proto)) {
        absl::StrAppend(output, " ", exported_variable_names_[var_index], "\n");
      }
    }
  }
  absl::StrAppend(output, "End\n");
  return true;
}

void MPModelProtoExporter::AppendMpsPair(const std::string& name, double value,
                                         std::string* output) const {
  absl::StrAppendFormat(output, *mps_format_, name, DoubleToString(value));
}

void MPModelProtoExporter::AppendMpsLineHeader(const std::string& id,
                                               const std::string& name,
                                               std::string* output) const {
  absl::StrAppendFormat(output, *mps_header_format_, id, name);
}

void MPModelProtoExporter::AppendMpsLineHeaderWithNewLine(
    const std::string& id, const std::string& name, std::string* output) const {
  AppendMpsLineHeader(id, name, output);
  absl::StripTrailingAsciiWhitespace(output);
  absl::StrAppend(output, "\n");
}

void MPModelProtoExporter::AppendMpsTermWithContext(
    const std::string& head_name, const std::string& name, double value,
    std::string* output) {
  if (current_mps_column_ == 0) {
    AppendMpsLineHeader("", head_name, output);
  }
  AppendMpsPair(name, value, output);
  AppendNewLineIfTwoColumns(output);
}

void MPModelProtoExporter::AppendMpsBound(const std::string& bound_type,
                                          const std::string& name, double value,
                                          std::string* output) const {
  AppendMpsLineHeader(bound_type, "BOUND", output);
  AppendMpsPair(name, value, output);
  absl::StripTrailingAsciiWhitespace(output);
  absl::StrAppend(output, "\n");
}

void MPModelProtoExporter::AppendNewLineIfTwoColumns(std::string* output) {
  ++current_mps_column_;
  if (current_mps_column_ == 2) {
    absl::StripTrailingAsciiWhitespace(output);
    absl::StrAppend(output, "\n");
    current_mps_column_ = 0;
  }
}

void MPModelProtoExporter::AppendMpsColumns(
    bool integrality,
    const std::vector<std::vector<std::pair<int, double>>>& transpose,
    std::string* output) {
  current_mps_column_ = 0;
  for (int var_index = 0; var_index < proto_.variable_size(); ++var_index) {
    const MPVariableProto& var_proto = proto_.variable(var_index);
    if (var_proto.is_integer() != integrality) continue;
    const std::string& var_name = exported_variable_names_[var_index];
    current_mps_column_ = 0;
    if (var_proto.objective_coefficient() != 0.0) {
      AppendMpsTermWithContext(var_name, "COST",
                               var_proto.objective_coefficient(), output);
    }
    for (const std::pair<int, double> cst_index_and_coeff :
         transpose[var_index]) {
      const std::string& cst_name =
          exported_constraint_names_[cst_index_and_coeff.first];
      AppendMpsTermWithContext(var_name, cst_name, cst_index_and_coeff.second,
                               output);
    }
    AppendNewLineIfTwoColumns(output);
  }
}

bool MPModelProtoExporter::ExportModelAsMpsFormat(
    const MPModelExportOptions& options, std::string* output) {
  output->clear();
  Setup();
  ComputeMpsSmartColumnWidths(options.obfuscate);
  const std::string kForbiddenFirstChars = "";
  const std::string kForbiddenChars = " ";
  exported_constraint_names_ = ExtractAndProcessNames(
      proto_.constraint(), "C", options.obfuscate, options.log_invalid_names,
      kForbiddenFirstChars, kForbiddenChars);
  exported_variable_names_ = ExtractAndProcessNames(
      proto_.variable(), "V", options.obfuscate, options.log_invalid_names,
      kForbiddenFirstChars, kForbiddenChars);

  if (proto_.maximize()) {
    LOG(DFATAL) << "MPS cannot represent maximization objectives.";
    return false;
  }
  // Comments.
  AppendComments("*", output);

  // NAME section.
  absl::StrAppendFormat(output, "%-14s%s\n", "NAME", proto_.name());

  // ROWS section.
  current_mps_column_ = 0;
  std::string rows_section;
  AppendMpsLineHeaderWithNewLine("N", "COST", &rows_section);
  for (int cst_index = 0; cst_index < proto_.constraint_size(); ++cst_index) {
    const MPConstraintProto& ct_proto = proto_.constraint(cst_index);
    const double lb = ct_proto.lower_bound();
    const double ub = ct_proto.upper_bound();
    const std::string& cst_name = exported_constraint_names_[cst_index];
    if (lb == -kInfinity && ub == kInfinity) {
      AppendMpsLineHeaderWithNewLine("N", cst_name, &rows_section);
    } else if (lb == ub) {
      AppendMpsLineHeaderWithNewLine("E", cst_name, &rows_section);
    } else if (lb == -kInfinity) {
      AppendMpsLineHeaderWithNewLine("L", cst_name, &rows_section);
    } else {
      AppendMpsLineHeaderWithNewLine("G", cst_name, &rows_section);
    }
  }
  if (!rows_section.empty()) {
    absl::StrAppend(output, "ROWS\n", rows_section);
  }

  // As the information regarding a column needs to be contiguous, we create
  // a vector associating a variable index to a vector containing the indices
  // of the constraints where this variable appears.
  std::vector<std::vector<std::pair<int, double>>> transpose(
      proto_.variable_size());
  for (int cst_index = 0; cst_index < proto_.constraint_size(); ++cst_index) {
    const MPConstraintProto& ct_proto = proto_.constraint(cst_index);
    for (int k = 0; k < ct_proto.var_index_size(); ++k) {
      const int var_index = ct_proto.var_index(k);
      if (var_index < 0 || var_index >= proto_.variable_size()) {
        LOG(DFATAL) << "In constraint #" << cst_index << ", var_index #" << k
                    << " is " << var_index << ", which is out of bounds.";
        return false;
      }
      const double coeff = ct_proto.coefficient(k);
      if (coeff != 0.0) {
        transpose[var_index].push_back(
            std::pair<int, double>(cst_index, coeff));
      }
    }
  }

  // COLUMNS section.
  std::string columns_section;
  AppendMpsColumns(/*integrality=*/true, transpose, &columns_section);
  if (!columns_section.empty()) {
    constexpr const char kIntMarkerFormat[] = "  %-10s%-36s%-8s\n";
    columns_section =
        absl::StrFormat(kIntMarkerFormat, "INTSTART", "'MARKER'", "'INTORG'") +
        columns_section;
    absl::StrAppendFormat(&columns_section, kIntMarkerFormat, "INTEND",
                          "'MARKER'", "'INTEND'");
  }
  AppendMpsColumns(/*integrality=*/false, transpose, &columns_section);
  if (!columns_section.empty()) {
    absl::StrAppend(output, "COLUMNS\n", columns_section);
  }

  // RHS (right-hand-side) section.
  current_mps_column_ = 0;
  std::string rhs_section;
  for (int cst_index = 0; cst_index < proto_.constraint_size(); ++cst_index) {
    const MPConstraintProto& ct_proto = proto_.constraint(cst_index);
    const double lb = ct_proto.lower_bound();
    const double ub = ct_proto.upper_bound();
    const std::string& cst_name = exported_constraint_names_[cst_index];
    if (lb != -kInfinity) {
      AppendMpsTermWithContext("RHS", cst_name, lb, &rhs_section);
    } else if (ub != +kInfinity) {
      AppendMpsTermWithContext("RHS", cst_name, ub, &rhs_section);
    }
  }
  AppendNewLineIfTwoColumns(&rhs_section);
  if (!rhs_section.empty()) {
    absl::StrAppend(output, "RHS\n", rhs_section);
  }

  // RANGES section.
  current_mps_column_ = 0;
  std::string ranges_section;
  for (int cst_index = 0; cst_index < proto_.constraint_size(); ++cst_index) {
    const MPConstraintProto& ct_proto = proto_.constraint(cst_index);
    const double range = fabs(ct_proto.upper_bound() - ct_proto.lower_bound());
    if (range != 0.0 && range != +kInfinity) {
      const std::string& cst_name = exported_constraint_names_[cst_index];
      AppendMpsTermWithContext("RANGE", cst_name, range, &ranges_section);
    }
  }
  AppendNewLineIfTwoColumns(&ranges_section);
  if (!ranges_section.empty()) {
    absl::StrAppend(output, "RANGES\n", ranges_section);
  }

  // BOUNDS section.
  current_mps_column_ = 0;
  std::string bounds_section;
  for (int var_index = 0; var_index < proto_.variable_size(); ++var_index) {
    const MPVariableProto& var_proto = proto_.variable(var_index);
    const double lb = var_proto.lower_bound();
    const double ub = var_proto.upper_bound();
    const std::string& var_name = exported_variable_names_[var_index];

    if (lb == -kInfinity && ub == +kInfinity) {
      AppendMpsLineHeader("FR", "BOUND", &bounds_section);
      absl::StrAppendFormat(&bounds_section, "  %s\n", var_name);
      continue;
    }

    if (var_proto.is_integer()) {
      if (IsBoolean(var_proto)) {
        AppendMpsLineHeader("BV", "BOUND", &bounds_section);
        absl::StrAppendFormat(&bounds_section, "  %s\n", var_name);
      } else {
        if (lb == -kInfinity && ub > 0) {
          // Non-standard MPS use seen on miplib2017/ns1456591 and adopted.
          // "MI" (indicating [-inf, 0] bounds) is supposed to be used only for
          // continuous variables, but solvers seem to read it as expected.
          AppendMpsLineHeader("MI", "BOUND", &bounds_section);
          absl::StrAppendFormat(&bounds_section, "  %s\n", var_name);
        }
        // "LI" can be skipped if it's -inf, or if it's 0.
        // There is one exception to that rule: if UI=+inf, we can't skip LI=0
        // or the variable will be parsed as binary.
        if (lb != -kInfinity && (lb != 0.0 || ub == kInfinity)) {
          AppendMpsBound("LI", var_name, lb, &bounds_section);
        }
        if (ub != kInfinity) {
          AppendMpsBound("UI", var_name, ub, &bounds_section);
        }
      }
    } else {
      if (lb == ub) {
        AppendMpsBound("FX", var_name, lb, &bounds_section);
      } else {
        if (lb != 0.0) {
          AppendMpsBound("LO", var_name, lb, &bounds_section);
        } else if (ub == +kInfinity) {
          AppendMpsLineHeader("PL", "BOUND", &bounds_section);
          absl::StrAppendFormat(&bounds_section, "  %s\n", var_name);
        }
        if (ub != +kInfinity) {
          AppendMpsBound("UP", var_name, ub, &bounds_section);
        }
      }
    }
  }
  if (!bounds_section.empty()) {
    absl::StrAppend(output, "BOUNDS\n", bounds_section);
  }

  absl::StrAppend(output, "ENDATA\n");
  return true;
}

}  // namespace
}  // namespace operations_research
