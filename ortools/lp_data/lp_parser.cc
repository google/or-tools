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

#include "ortools/lp_data/lp_parser.h"

#include <set>
#include <string>
#include <vector>

#include "absl/container/flat_hash_set.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/match.h"
#include "absl/strings/numbers.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_split.h"
#include "absl/strings/string_view.h"
#include "absl/strings/strip.h"
#include "ortools/base/case.h"
#include "ortools/base/map_util.h"
#include "ortools/linear_solver/linear_solver.pb.h"
#include "ortools/lp_data/proto_utils.h"
#include "re2/re2.h"

namespace operations_research {
namespace glop {

namespace {

using ::absl::StatusOr;

enum class TokenType {
  ERROR,
  END,
  ADDAND,
  VALUE,
  INF,
  NAME,
  SIGN_LE,
  SIGN_EQ,
  SIGN_GE,
  COMA,
};

bool TokenIsBound(TokenType token_type) {
  if (token_type == TokenType::VALUE || token_type == TokenType::INF) {
    return true;
  }
  return false;
}

// Not thread safe.
class LPParser {
 public:
  // Accepts the string in LP file format (used by LinearProgram::Dump()).
  // On success, populates the linear program *lp and returns true. Otherwise,
  // returns false and leaves *lp in an unspecified state.
  ABSL_MUST_USE_RESULT bool Parse(absl::string_view model, LinearProgram* lp);

 private:
  bool ParseEmptyLine(absl::string_view line);
  bool ParseObjective(absl::string_view objective);
  bool ParseIntegerVariablesList(absl::string_view line);
  bool ParseConstraint(absl::string_view constraint);
  TokenType ConsumeToken(absl::string_view* sp);
  bool SetVariableBounds(ColIndex col, Fractional lb, Fractional ub);

  // Linear program populated by the Parse() method. Not owned.
  LinearProgram* lp_;

  // Contains the last consumed coefficient and name. The name can be the
  // optimization direction, a constraint name, or a variable name.
  Fractional consumed_coeff_;
  std::string consumed_name_;

  // To remember whether the variable bounds had already been set.
  std::set<ColIndex> bounded_variables_;
};

bool LPParser::Parse(absl::string_view model, LinearProgram* lp) {
  lp_ = lp;
  bounded_variables_.clear();
  lp_->Clear();

  std::vector<absl::string_view> lines =
      absl::StrSplit(model, ';', absl::SkipEmpty());
  bool has_objective = false;

  for (absl::string_view line : lines) {
    if (!has_objective && ParseObjective(line)) {
      has_objective = true;
    } else if (!ParseConstraint(line) && !ParseIntegerVariablesList(line) &&
               !ParseEmptyLine(line)) {
      LOG(INFO) << "Error in line: " << line;
      return false;
    }
  }

  // Bound the non-bounded variables between -inf and +inf. We need to do this,
  // as glop bounds a variable by default between 0 and +inf.
  for (ColIndex col(0); col < lp_->num_variables(); ++col) {
    if (bounded_variables_.find(col) == bounded_variables_.end()) {
      lp_->SetVariableBounds(col, -kInfinity, +kInfinity);
    }
  }

  lp_->CleanUp();
  return true;
}

bool LPParser::ParseEmptyLine(absl::string_view line) {
  if (ConsumeToken(&line) == TokenType::END) return true;
  return false;
}

bool LPParser::ParseObjective(absl::string_view objective) {
  // Get the required optimization direction.
  if (ConsumeToken(&objective) != TokenType::NAME) return false;
  if (absl::EqualsIgnoreCase(consumed_name_, "min")) {
    lp_->SetMaximizationProblem(false);
  } else if (absl::EqualsIgnoreCase(consumed_name_, "max")) {
    lp_->SetMaximizationProblem(true);
  } else {
    return false;
  }

  // Get the optional offset.
  TokenType token_type = ConsumeToken(&objective);
  if (token_type == TokenType::VALUE) {
    lp_->SetObjectiveOffset(consumed_coeff_);
    token_type = ConsumeToken(&objective);
  } else {
    lp_->SetObjectiveOffset(0.0);
  }

  // Get the addands.
  while (token_type == TokenType::ADDAND) {
    const ColIndex col = lp_->FindOrCreateVariable(consumed_name_);
    if (lp_->objective_coefficients()[col] != 0.0) return false;
    lp_->SetObjectiveCoefficient(col, consumed_coeff_);
    token_type = ConsumeToken(&objective);
  }
  return token_type == TokenType::END;
}

bool LPParser::ParseIntegerVariablesList(absl::string_view line) {
  // Get the required "int" or "bin" keyword.
  bool binary_list = false;
  if (ConsumeToken(&line) != TokenType::NAME) return false;
  if (absl::EqualsIgnoreCase(consumed_name_, "bin")) {
    binary_list = true;
  } else if (!absl::EqualsIgnoreCase(consumed_name_, "int")) {
    return false;
  }

  // Get the list of integer variables, separated by optional comas.
  TokenType token_type = ConsumeToken(&line);
  while (token_type == TokenType::ADDAND) {
    if (consumed_coeff_ != 1.0) return false;
    const ColIndex col = lp_->FindOrCreateVariable(consumed_name_);
    lp_->SetVariableType(col, LinearProgram::VariableType::INTEGER);
    if (binary_list && !SetVariableBounds(col, 0.0, 1.0)) return false;
    token_type = ConsumeToken(&line);
    if (token_type == TokenType::COMA) {
      token_type = ConsumeToken(&line);
    }
  }

  // The last token must be END.
  if (token_type != TokenType::END) return false;
  return true;
}

bool LPParser::ParseConstraint(absl::string_view constraint) {
  const StatusOr<ParsedConstraint> parsed_constraint_or_status =
      ::operations_research::glop::ParseConstraint(constraint);
  if (!parsed_constraint_or_status.ok()) return false;
  const ParsedConstraint& parsed_constraint =
      parsed_constraint_or_status.value();

  // Set the variables bounds without creating new constraints.
  if (parsed_constraint.name.empty() &&
      parsed_constraint.coefficients.size() == 1 &&
      parsed_constraint.coefficients[0] == 1.0) {
    const ColIndex col =
        lp_->FindOrCreateVariable(parsed_constraint.variable_names[0]);
    if (!SetVariableBounds(col, parsed_constraint.lower_bound,
                           parsed_constraint.upper_bound)) {
      return false;
    }
  } else {
    const RowIndex num_constraints_before_adding_variable =
        lp_->num_constraints();
    // The constaint has a name, or there are more than variable, or the
    // coefficient is not 1. Thus, create and fill a new constraint.
    // We don't use SetConstraintName() because constraints named that way
    // cannot be found via FindOrCreateConstraint() (see comment on
    // SetConstraintName()), which can be useful for tests using ParseLP.
    const RowIndex row =
        parsed_constraint.name.empty()
            ? lp_->CreateNewConstraint()
            : lp_->FindOrCreateConstraint(parsed_constraint.name);
    if (lp_->num_constraints() == num_constraints_before_adding_variable) {
      // No constraints were added, meaning we found one.
      LOG(INFO) << "Two constraints with the same name: "
                << parsed_constraint.name;
      return false;
    }
    if (!AreBoundsValid(parsed_constraint.lower_bound,
                        parsed_constraint.upper_bound)) {
      return false;
    }
    lp_->SetConstraintBounds(row, parsed_constraint.lower_bound,
                             parsed_constraint.upper_bound);
    for (int i = 0; i < parsed_constraint.variable_names.size(); ++i) {
      const ColIndex variable =
          lp_->FindOrCreateVariable(parsed_constraint.variable_names[i]);
      lp_->SetCoefficient(row, variable, parsed_constraint.coefficients[i]);
    }
  }
  return true;
}

bool LPParser::SetVariableBounds(ColIndex col, Fractional lb, Fractional ub) {
  if (bounded_variables_.find(col) == bounded_variables_.end()) {
    // The variable was not bounded yet, thus reset its bounds.
    bounded_variables_.insert(col);
    lp_->SetVariableBounds(col, -kInfinity, kInfinity);
  }
  // Set the bounds only if their stricter and valid.
  lb = std::max(lb, lp_->variable_lower_bounds()[col]);
  ub = std::min(ub, lp_->variable_upper_bounds()[col]);
  if (!AreBoundsValid(lb, ub)) return false;
  lp_->SetVariableBounds(col, lb, ub);
  return true;
}

TokenType ConsumeToken(absl::string_view* sp, std::string* consumed_name,
                       double* consumed_coeff) {
  DCHECK(consumed_name != nullptr);
  DCHECK(consumed_coeff != nullptr);
  // We use LazyRE2 everywhere so that all the patterns are just compiled once
  // when they are needed for the first time. This speed up the code
  // significantly. Note that the use of LazyRE2 is thread safe.
  static const LazyRE2 kEndPattern = {R"(\s*)"};

  // There is nothing more to consume.
  if (sp->empty() || RE2::FullMatch(*sp, *kEndPattern)) {
    return TokenType::END;
  }

  // Return NAME if the next token is a line name, or integer variable list
  // indicator.
  static const LazyRE2 kNamePattern1 = {R"(\s*(\w[\w[\]]*):)"};
  static const LazyRE2 kNamePattern2 = {R"((?i)\s*(int)\s*:?)"};
  static const LazyRE2 kNamePattern3 = {R"((?i)\s*(bin)\s*:?)"};
  if (RE2::Consume(sp, *kNamePattern1, consumed_name)) return TokenType::NAME;
  if (RE2::Consume(sp, *kNamePattern2, consumed_name)) return TokenType::NAME;
  if (RE2::Consume(sp, *kNamePattern3, consumed_name)) return TokenType::NAME;

  // Return SIGN_* if the next token is a relation sign.
  static const LazyRE2 kLePattern = {R"(\s*<=?)"};
  if (RE2::Consume(sp, *kLePattern)) return TokenType::SIGN_LE;
  static const LazyRE2 kEqPattern = {R"(\s*=)"};
  if (RE2::Consume(sp, *kEqPattern)) return TokenType::SIGN_EQ;
  static const LazyRE2 kGePattern = {R"(\s*>=?)"};
  if (RE2::Consume(sp, *kGePattern)) return TokenType::SIGN_GE;

  // Return COMA if the next token is a coma.
  static const LazyRE2 kComaPattern = {R"(\s*\,)"};
  if (RE2::Consume(sp, *kComaPattern)) return TokenType::COMA;

  // Consume all plus and minus signs.
  std::string sign;
  int minus_count = 0;
  static const LazyRE2 kSignPattern = {R"(\s*([-+]{1}))"};
  while (RE2::Consume(sp, *kSignPattern, &sign)) {
    if (sign == "-") minus_count++;
  }

  // Return INF if the next token is an infinite value.
  static const LazyRE2 kInfPattern = {R"((?i)\s*inf)"};
  if (RE2::Consume(sp, *kInfPattern)) {
    *consumed_coeff = minus_count % 2 == 0 ? kInfinity : -kInfinity;
    return TokenType::INF;
  }

  // Check if the next token is a value. If it is infinite return INF.
  std::string coeff;
  bool has_value = false;
  static const LazyRE2 kValuePattern = {
      R"(\s*([0-9]*\.?[0-9]+([eE][-+]?[0-9]+)?))"};
  if (RE2::Consume(sp, *kValuePattern, &coeff)) {
    if (!absl::SimpleAtod(coeff, consumed_coeff)) {
      // Note: If absl::SimpleAtod(), Consume(), and kValuePattern are correct,
      // this should never happen.
      LOG(ERROR) << "Text: " << coeff << " was matched by RE2 to be "
                 << "a floating point number, but absl::SimpleAtod() failed.";
      return TokenType::ERROR;
    }
    if (!IsFinite(*consumed_coeff)) {
      VLOG(1) << "Value " << coeff << " treated as infinite.";
      return TokenType::INF;
    }
    has_value = true;
  } else {
    *consumed_coeff = 1.0;
  }
  if (minus_count % 2 == 1) *consumed_coeff *= -1.0;

  // Return ADDAND (coefficient and name) if the next token is a variable name.
  // Otherwise, if we found a finite value previously, return VALUE.
  // Otherwise, return ERROR.
  std::string multiplication;
  static const LazyRE2 kAddandPattern = {R"(\s*(\*?)\s*([a-zA-Z_)][\w[\])]*))"};
  if (RE2::Consume(sp, *kAddandPattern, &multiplication, consumed_name)) {
    if (!multiplication.empty() && !has_value) return TokenType::ERROR;
    return TokenType::ADDAND;
  } else if (has_value) {
    return TokenType::VALUE;
  }

  return TokenType::ERROR;
}

TokenType LPParser::ConsumeToken(absl::string_view* sp) {
  using ::operations_research::glop::ConsumeToken;
  return ConsumeToken(sp, &consumed_name_, &consumed_coeff_);
}

}  // namespace

StatusOr<ParsedConstraint> ParseConstraint(absl::string_view constraint) {
  ParsedConstraint parsed_constraint;
  // Get the name, if present.
  absl::string_view constraint_copy(constraint);
  std::string consumed_name;
  Fractional consumed_coeff;
  if (ConsumeToken(&constraint_copy, &consumed_name, &consumed_coeff) ==
      TokenType::NAME) {
    parsed_constraint.name = consumed_name;
    constraint = constraint_copy;
  }

  Fractional left_bound;
  Fractional right_bound;
  TokenType left_sign(TokenType::END);
  TokenType right_sign(TokenType::END);
  absl::flat_hash_set<std::string> used_variables;

  // Get the left bound and the relation sign, if present.
  TokenType token_type =
      ConsumeToken(&constraint, &consumed_name, &consumed_coeff);
  if (TokenIsBound(token_type)) {
    left_bound = consumed_coeff;
    left_sign = ConsumeToken(&constraint, &consumed_name, &consumed_coeff);
    if (left_sign != TokenType::SIGN_LE && left_sign != TokenType::SIGN_EQ &&
        left_sign != TokenType::SIGN_GE) {
      return absl::InvalidArgumentError(
          "Expected an equality/inequality sign for the left bound.");
    }
    token_type = ConsumeToken(&constraint, &consumed_name, &consumed_coeff);
  }

  // Get the addands, if present.
  while (token_type == TokenType::ADDAND) {
    if (used_variables.contains(consumed_name)) {
      return absl::InvalidArgumentError(
          absl::StrCat("Duplicate variable name: ", consumed_name));
    }
    used_variables.insert(consumed_name);
    parsed_constraint.variable_names.push_back(consumed_name);
    parsed_constraint.coefficients.push_back(consumed_coeff);
    token_type = ConsumeToken(&constraint, &consumed_name, &consumed_coeff);
  }

  // If the left sign was EQ there can be no right side.
  if (left_sign == TokenType::SIGN_EQ && token_type != TokenType::END) {
    return absl::InvalidArgumentError(
        "Equality constraints can have only one bound.");
  }

  // Get the right sign and the right bound, if present.
  if (token_type != TokenType::END) {
    right_sign = token_type;
    if (right_sign != TokenType::SIGN_LE && right_sign != TokenType::SIGN_EQ &&
        right_sign != TokenType::SIGN_GE) {
      return absl::InvalidArgumentError(
          "Expected an equality/inequality sign for the right bound.");
    }
    // If the right sign is EQ, there can be no left side.
    if (left_sign != TokenType::END && right_sign == TokenType::SIGN_EQ) {
      return absl::InvalidArgumentError(
          "Equality constraints can have only one bound.");
    }
    if (!TokenIsBound(
            ConsumeToken(&constraint, &consumed_name, &consumed_coeff))) {
      return absl::InvalidArgumentError("Bound value was expected.");
    }
    right_bound = consumed_coeff;
    if (ConsumeToken(&constraint, &consumed_name, &consumed_coeff) !=
        TokenType::END) {
      return absl::InvalidArgumentError(
          absl::StrCat("End of input was expected, found: ", constraint));
    }
  }

  // There was no constraint!
  if (left_sign == TokenType::END && right_sign == TokenType::END) {
    return absl::InvalidArgumentError("The input constraint was empty.");
  }

  // Calculate bounds to set.
  parsed_constraint.lower_bound = -kInfinity;
  parsed_constraint.upper_bound = kInfinity;
  if (left_sign == TokenType::SIGN_LE || left_sign == TokenType::SIGN_EQ) {
    parsed_constraint.lower_bound = left_bound;
  }
  if (left_sign == TokenType::SIGN_GE || left_sign == TokenType::SIGN_EQ) {
    parsed_constraint.upper_bound = left_bound;
  }
  if (right_sign == TokenType::SIGN_LE || right_sign == TokenType::SIGN_EQ) {
    parsed_constraint.upper_bound =
        std::min(parsed_constraint.upper_bound, right_bound);
  }
  if (right_sign == TokenType::SIGN_GE || right_sign == TokenType::SIGN_EQ) {
    parsed_constraint.lower_bound =
        std::max(parsed_constraint.lower_bound, right_bound);
  }
  return parsed_constraint;
}

bool ParseLp(absl::string_view model, LinearProgram* lp) {
  LPParser parser;
  return parser.Parse(model, lp);
}

}  // namespace glop

absl::StatusOr<MPModelProto> ModelProtoFromLpFormat(absl::string_view model) {
  glop::LinearProgram lp;
  if (!ParseLp(model, &lp)) {
    return absl::InvalidArgumentError("Parsing error, see LOGs for details.");
  }
  MPModelProto model_proto;
  LinearProgramToMPModelProto(lp, &model_proto);
  return model_proto;
}

}  // namespace operations_research
