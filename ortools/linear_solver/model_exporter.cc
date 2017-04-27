// Copyright 2010-2014 Google
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

#include <cmath>
#include <limits>
#include <unordered_set>

#include "ortools/base/commandlineflags.h"
#include "ortools/base/integral_types.h"
#include "ortools/base/logging.h"
#include "ortools/base/stringprintf.h"
#include "ortools/base/join.h"
#include "ortools/base/strutil.h"
#include "ortools/base/map_util.h"
#include "ortools/linear_solver/linear_solver.pb.h"
#include "ortools/util/fp_utils.h"

DEFINE_bool(lp_shows_unused_variables, false,
            "Decides wether variable unused in the objective and constraints"
            " are shown when exported to a file using the lp format.");

DEFINE_int32(lp_max_line_length, 10000,
             "Maximum line length in exported .lp files. The default was chosen"
             " so that SCIP can read the files.");

DEFINE_bool(lp_log_invalid_name, false,
            "Whether to log invalid variable and contraint names.");

namespace operations_research {

MPModelProtoExporter::MPModelProtoExporter(const MPModelProto& proto)
    : proto_(proto),
      num_integer_variables_(0),
      num_binary_variables_(0),
      num_continuous_variables_(0),
      current_mps_column_(0),
      use_fixed_mps_format_(false),
      use_obfuscated_names_(false) {}

namespace {
class NameManager {
 public:
  NameManager() : names_set_(), last_n_(1) {}
  std::string MakeUniqueName(const std::string& name);

 private:
  std::unordered_set<std::string> names_set_;
  int last_n_;
};

std::string NameManager::MakeUniqueName(const std::string& name) {
  std::string result = name;
  // Find the 'n' so that "name_n" does not already exist.
  int n = last_n_;
  while (!names_set_.insert(result).second) {
    result = StrCat(name, "_", n);
    ++n;
  }
  // We keep the last n used to avoid a quadratic behavior in case
  // all the names are the same initially.
  last_n_ = n;
  return result;
}

std::string MakeExportableName(const std::string& name, bool* found_forbidden_char) {
  // Prepend with "_" all the names starting with a forbidden character.
  const std::string kForbiddenFirstChars = "$.0123456789";
  *found_forbidden_char = kForbiddenFirstChars.find(name[0]) != std::string::npos;
  std::string exportable_name = *found_forbidden_char ? StrCat("_", name) : name;

  // Replace all the other forbidden characters with "_".
  const std::string kForbiddenChars = " +-*/<>=:\\";
  for (char& c : exportable_name) {
    if (kForbiddenChars.find(c) != std::string::npos) {
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
    bool obfuscate) {
  const int num_items = proto.size();
  std::vector<std::string> result(num_items);
  NameManager namer;
  const int num_digits = StrCat(num_items).size();
  int i = 0;
  for (const auto& item : proto) {
    const std::string obfuscated_name =
        StringPrintf("%s%0*d", prefix.c_str(), num_digits, i);
    if (obfuscate || !item.has_name()) {
      result[i] = namer.MakeUniqueName(obfuscated_name);
      LOG_IF(WARNING, FLAGS_lp_log_invalid_name && !item.has_name())
          << "Empty name detected, created new name: " << result[i];
    } else {
      bool found_forbidden_char = false;
      const std::string exportable_name =
          MakeExportableName(item.name(), &found_forbidden_char);
      result[i] = namer.MakeUniqueName(exportable_name);
      LOG_IF(WARNING, FLAGS_lp_log_invalid_name && found_forbidden_char)
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
        LOG_IF(WARNING, FLAGS_lp_log_invalid_name)
            << "Name is too long: " << old_name
            << " exported as: " << result[i];
      }
    }
    // Update whether we can use the names in a fixed-format MPS file.
    const int kFixedMpsFieldSize = 8;
    use_fixed_mps_format_ =
        use_fixed_mps_format_ && (result[i].size() <= kFixedMpsFieldSize);

    // Prepare for the next round.
    ++i;
  }
  return result;
}

void MPModelProtoExporter::AppendComments(const std::string& separator,
                                          std::string* output) const {
  const char* const sep = separator.c_str();
  StringAppendF(output, "%s Generated by MPModelProtoExporter\n", sep);
  StringAppendF(output, "%s   %-16s : %s\n", sep, "Name",
                proto_.has_name() ? proto_.name().c_str() : "NoName");
  StringAppendF(output, "%s   %-16s : %s\n", sep, "Format",
                use_fixed_mps_format_ ? "Fixed" : "Free");
  StringAppendF(output, "%s   %-16s : %d\n", sep, "Constraints",
                proto_.constraint_size());
  StringAppendF(output, "%s   %-16s : %d\n", sep, "Variables",
                proto_.variable_size());
  StringAppendF(output, "%s     %-14s : %d\n", sep, "Binary",
                num_binary_variables_);
  StringAppendF(output, "%s     %-14s : %d\n", sep, "Integer",
                num_integer_variables_);
  StringAppendF(output, "%s     %-14s : %d\n", sep, "Continuous",
                num_continuous_variables_);
  if (FLAGS_lp_shows_unused_variables) {
    StringAppendF(output, "%s Unused variables are shown\n", sep);
  }
}

namespace {
class LineBreaker {
 public:
  explicit LineBreaker(int max_line_size) :
      max_line_size_(max_line_size), line_size_(0), output_() {}
  // Lines are broken in such a way that:
  // - Strings that are given to Append() are never split.
  // - Lines are split so that their length doesn't exceed the max length;
  //   unless a single std::string given to Append() exceeds that length (in which
  //   case it will be put alone on a single unsplit line).
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
    StrAppend(&output_, "\n ");
  }
  StrAppend(&output_, s);
}

}  // namespace

bool MPModelProtoExporter::WriteLpTerm(int var_index, double coefficient,
                                       std::string* output) const {
  output->clear();
  if (var_index < 0 || var_index >= proto_.variable_size()) {
    LOG(DFATAL) << "Reference to out-of-bounds variable index # " << var_index;
    return false;
  }
  if (coefficient != 0.0) {
    *output = StringPrintf("%+.16G %-s ", coefficient,
                           exported_variable_names_[var_index].c_str());
  }
  return true;
}

namespace {
bool IsBoolean(const MPVariableProto& var) {
  return var.is_integer() && ceil(var.lower_bound()) == 0.0 &&
         floor(var.upper_bound()) == 1.0;
}
}  // namespace

void MPModelProtoExporter::Setup() {
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

bool MPModelProtoExporter::ExportModelAsLpFormat(bool obfuscated,
                                                 std::string* output) {
  output->clear();
  Setup();
  exported_constraint_names_ =
      ExtractAndProcessNames(proto_.constraint(), "C", obfuscated);
  exported_variable_names_ =
      ExtractAndProcessNames(proto_.variable(), "V", obfuscated);

  // Comments section.
  AppendComments("\\", output);

  // Objective
  StrAppend(output, proto_.maximize() ? "Maximize\n" : "Minimize\n");
  LineBreaker obj_line_breaker(FLAGS_lp_max_line_length);
  obj_line_breaker.Append(" Obj: ");
  if (proto_.objective_offset() != 0.0) {
    obj_line_breaker.Append(StringPrintf("%-+.16G Constant ",
                                         proto_.objective_offset()));
  }
  std::vector<bool> show_variable(proto_.variable_size(),
                                  FLAGS_lp_shows_unused_variables);
  for (int var_index = 0; var_index < proto_.variable_size(); ++var_index) {
    const double coeff = proto_.variable(var_index).objective_coefficient();
    std::string term;
    if (!WriteLpTerm(var_index, coeff, &term)) {
      return false;
    }
    obj_line_breaker.Append(term);
    show_variable[var_index] = coeff != 0.0 || FLAGS_lp_shows_unused_variables;
  }
  // Constraints
  StrAppend(output, obj_line_breaker.GetOutput(), "\nSubject to\n");
  for (int cst_index = 0; cst_index < proto_.constraint_size(); ++cst_index) {
    const MPConstraintProto& ct_proto = proto_.constraint(cst_index);
    const std::string& name = exported_constraint_names_[cst_index];
    LineBreaker line_breaker(FLAGS_lp_max_line_length);
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
      show_variable[var_index] =
          coeff != 0.0 || FLAGS_lp_shows_unused_variables;
    }
    const double lb = ct_proto.lower_bound();
    const double ub = ct_proto.upper_bound();
    if (lb == ub) {
      line_breaker.Append(StringPrintf(" = %-.16G\n", ub));
      StrAppend(output, " ", name, ": ", line_breaker.GetOutput());
    } else {
      if (ub != +std::numeric_limits<double>::infinity()) {
        std::string rhs_name = name;
        if (lb != -std::numeric_limits<double>::infinity()) {
          rhs_name += "_rhs";
        }
        StrAppend(output, " ", rhs_name, ": ", line_breaker.GetOutput());
        const std::string relation = StringPrintf(" <= %-.16G\n", ub);
        // Here we have to make sure we do not add the relation to the contents
        // of line_breaker, which may be used in the subsequent clause.
        if (!line_breaker.WillFit(relation)) StrAppend(output, "\n ");
        StrAppend(output, relation);
      }
      if (lb != -std::numeric_limits<double>::infinity()) {
        std::string lhs_name = name;
        if (ub != +std::numeric_limits<double>::infinity()) {
          lhs_name += "_lhs";
        }
        StrAppend(output, " ", lhs_name, ": ", line_breaker.GetOutput());
        const std::string relation = StringPrintf(" >= %-.16G\n", lb);
        if (!line_breaker.WillFit(relation)) StrAppend(output, "\n ");
        StrAppend(output, relation);
      }
    }
  }

  // Bounds
  output->append("Bounds\n");
  if (proto_.objective_offset() != 0.0) {
    output->append(" 1 <= Constant <= 1\n");
  }
  for (int var_index = 0; var_index < proto_.variable_size(); ++var_index) {
    if (!show_variable[var_index]) continue;
    const MPVariableProto& var_proto = proto_.variable(var_index);
    const double lb = var_proto.lower_bound();
    const double ub = var_proto.upper_bound();
    if (var_proto.is_integer() && lb == round(lb) && ub == round(ub)) {
      StringAppendF(output, " %.0f <= %s <= %.0f\n", lb,
                    exported_variable_names_[var_index].c_str(), ub);
    } else {
      if (lb != -std::numeric_limits<double>::infinity()) {
        StringAppendF(output, " %-.16G <= ", lb);
      }
      output->append(exported_variable_names_[var_index]);
      if (ub != std::numeric_limits<double>::infinity()) {
        StringAppendF(output, " <= %-.16G", ub);
      }
      output->append("\n");
    }
  }

  // Binaries
  if (num_binary_variables_ > 0) {
    output->append("Binaries\n");
    for (int var_index = 0; var_index < proto_.variable_size(); ++var_index) {
      if (!show_variable[var_index]) continue;
      const MPVariableProto& var_proto = proto_.variable(var_index);
      if (IsBoolean(var_proto)) {
        StringAppendF(output, " %s\n",
                      exported_variable_names_[var_index].c_str());
      }
    }
  }

  // Generals
  if (num_integer_variables_ > 0) {
    output->append("Generals\n");
    for (int var_index = 0; var_index < proto_.variable_size(); ++var_index) {
      if (!show_variable[var_index]) continue;
      const MPVariableProto& var_proto = proto_.variable(var_index);
      if (var_proto.is_integer() && !IsBoolean(var_proto)) {
        StringAppendF(output, " %s\n",
                      exported_variable_names_[var_index].c_str());
      }
    }
  }
  output->append("End\n");
  return true;
}

void MPModelProtoExporter::AppendMpsPair(const std::string& name, double value,
                                         std::string* output) const {
  const int kFixedMpsDoubleWidth = 12;
  if (use_fixed_mps_format_) {
    int precision = kFixedMpsDoubleWidth;
    std::string value_str = StringPrintf("%.*G", precision, value);
    // Use the largest precision that can fit into the field witdh.
    while (value_str.size() > kFixedMpsDoubleWidth) {
      --precision;
      value_str = StringPrintf("%.*G", precision, value);
    }
    StringAppendF(output, "  %-8s  %*s ", name.c_str(), kFixedMpsDoubleWidth,
                  value_str.c_str());
  } else {
    StringAppendF(output, "  %-16s  %21.16G ", name.c_str(), value);
  }
}

void MPModelProtoExporter::AppendMpsLineHeader(const std::string& id,
                                               const std::string& name,
                                               std::string* output) const {
  StringAppendF(output, use_fixed_mps_format_ ? " %-2s %-8s" : " %-2s  %-16s",
                id.c_str(), name.c_str());
}

void MPModelProtoExporter::AppendMpsLineHeaderWithNewLine(
    const std::string& id, const std::string& name, std::string* output) const {
  AppendMpsLineHeader(id, name, output);
  *output += "\n";
}

void MPModelProtoExporter::AppendMpsTermWithContext(const std::string& head_name,
                                                    const std::string& name,
                                                    double value,
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
  *output += "\n";
}

void MPModelProtoExporter::AppendNewLineIfTwoColumns(std::string* output) {
  ++current_mps_column_;
  if (current_mps_column_ == 2) {
    *output += "\n";
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
                               var_proto.objective_coefficient(),
                               output);
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

bool MPModelProtoExporter::ExportModelAsMpsFormat(bool fixed_format,
                                                  bool obfuscated,
                                                  std::string* output) {
  output->clear();
  Setup();
  use_fixed_mps_format_ = fixed_format;
  exported_constraint_names_ =
      ExtractAndProcessNames(proto_.constraint(), "C", obfuscated);
  exported_variable_names_ =
      ExtractAndProcessNames(proto_.variable(), "V", obfuscated);

  // use_fixed_mps_format_ was possibly modified by ExtractAndProcessNames().
  LOG_IF(WARNING, fixed_format && !use_fixed_mps_format_)
      << "Cannot use fixed format. Falling back to free format";
  // Comments.
  AppendComments("*", output);

  // NAME section.
  StringAppendF(output, "%-14s%s\n", "NAME", proto_.name().c_str());

  // ROWS section.
  current_mps_column_ = 0;
  std::string rows_section;
  AppendMpsLineHeaderWithNewLine("N", "COST", &rows_section);
  for (int cst_index = 0; cst_index < proto_.constraint_size(); ++cst_index) {
    const MPConstraintProto& ct_proto = proto_.constraint(cst_index);
    const double lb = ct_proto.lower_bound();
    const double ub = ct_proto.upper_bound();
    const std::string& cst_name = exported_constraint_names_[cst_index];
    if (lb == ub) {
      AppendMpsLineHeaderWithNewLine("E", cst_name, &rows_section);
    } else if (lb == -std::numeric_limits<double>::infinity()) {
      DCHECK_NE(std::numeric_limits<double>::infinity(), ub);
      AppendMpsLineHeaderWithNewLine("L", cst_name, &rows_section);
    } else {
      DCHECK_NE(-std::numeric_limits<double>::infinity(), lb);
      AppendMpsLineHeaderWithNewLine("G", cst_name, &rows_section);
    }
  }
  if (!rows_section.empty()) {
    *output += "ROWS\n" + rows_section;
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
    const char* const kIntMarkerFormat = "  %-10s%-36s%-10s\n";
    columns_section = StringPrintf(kIntMarkerFormat, "INTSTART",
                                   "'MARKER'", "'INTORG'") + columns_section;
    StringAppendF(&columns_section, kIntMarkerFormat,
                  "INTEND", "'MARKER'", "'INTEND'");
  }
  AppendMpsColumns(/*integrality=*/false, transpose, &columns_section);
  if (!columns_section.empty()) {
    *output += "COLUMNS\n" + columns_section;
  }

  // RHS (right-hand-side) section.
  current_mps_column_ = 0;
  std::string rhs_section;
  for (int cst_index = 0; cst_index < proto_.constraint_size(); ++cst_index) {
    const MPConstraintProto& ct_proto = proto_.constraint(cst_index);
    const double lb = ct_proto.lower_bound();
    const double ub = ct_proto.upper_bound();
    const std::string& cst_name = exported_constraint_names_[cst_index];
    if (lb != -std::numeric_limits<double>::infinity()) {
      AppendMpsTermWithContext("RHS", cst_name, lb, &rhs_section);
    } else if (ub != +std::numeric_limits<double>::infinity()) {
      AppendMpsTermWithContext("RHS", cst_name, ub, &rhs_section);
    }
  }
  AppendNewLineIfTwoColumns(&rhs_section);
  if (!rhs_section.empty()) {
    *output += "RHS\n" + rhs_section;
  }

  // RANGES section.
  current_mps_column_ = 0;
  std::string ranges_section;
  for (int cst_index = 0; cst_index < proto_.constraint_size(); ++cst_index) {
    const MPConstraintProto& ct_proto = proto_.constraint(cst_index);
    const double range = fabs(ct_proto.upper_bound() - ct_proto.lower_bound());
    if (range != 0.0 && range != +std::numeric_limits<double>::infinity()) {
      const std::string& cst_name = exported_constraint_names_[cst_index];
      AppendMpsTermWithContext("RANGE", cst_name, range, &ranges_section);
    }
  }
  AppendNewLineIfTwoColumns(&ranges_section);
  if (!ranges_section.empty()) {
    *output += "RANGES\n" + ranges_section;
  }

  // BOUNDS section.
  current_mps_column_ = 0;
  std::string bounds_section;
  for (int var_index = 0; var_index < proto_.variable_size(); ++var_index) {
    const MPVariableProto& var_proto = proto_.variable(var_index);
    const double lb = var_proto.lower_bound();
    const double ub = var_proto.upper_bound();
    const std::string& var_name = exported_variable_names_[var_index];
    if (var_proto.is_integer()) {
      if (IsBoolean(var_proto)) {
        AppendMpsLineHeader("BV", "BOUND", &bounds_section);
        StringAppendF(&bounds_section, "  %s\n", var_name.c_str());
      } else {
        if (lb != 0.0) {
          AppendMpsBound("LI", var_name, lb, &bounds_section);
        }
        if (ub != +std::numeric_limits<double>::infinity()) {
          AppendMpsBound("UI", var_name, ub, &bounds_section);
        }
      }
    } else {
      if (lb == -std::numeric_limits<double>::infinity() &&
          ub == +std::numeric_limits<double>::infinity()) {
        AppendMpsLineHeader("FR", "BOUND", &bounds_section);
        StringAppendF(&bounds_section, "  %s\n", var_name.c_str());
      } else if (lb == ub) {
        AppendMpsBound("FX", var_name, lb, &bounds_section);
      } else {
        if (lb != 0.0) {
          AppendMpsBound("LO", var_name, lb, &bounds_section);
        } else if (ub == +std::numeric_limits<double>::infinity()) {
          AppendMpsLineHeader("PL", "BOUND", &bounds_section);
          StringAppendF(&bounds_section, "  %s\n", var_name.c_str());
        }
        if (ub != +std::numeric_limits<double>::infinity()) {
          AppendMpsBound("UP", var_name, ub, &bounds_section);
        }
      }
    }
  }
  if (!bounds_section.empty()) {
    *output += "BOUNDS\n" + bounds_section;
  }

  *output += "ENDATA\n";
  return true;
}

}  // namespace operations_research
