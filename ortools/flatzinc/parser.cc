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

#include "ortools/flatzinc/parser.h"

#include <cstdint>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include "absl/container/flat_hash_map.h"
#include "absl/functional/overload.h"
#include "absl/log/check.h"
#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/strings/match.h"
#include "absl/strings/str_format.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "ortools/base/helpers.h"
#include "ortools/base/options.h"
#include "ortools/flatzinc/lexer.h"
#include "ortools/flatzinc/model.h"

namespace operations_research::fz {
namespace {

// Returns true if `annotations` contains a top-level identifier (`:: foo`) or
// function-call (`:: foo(...)`) annotation with name `id`.
bool ContainsAnnotationId(absl::Span<const Annotation> annotations,
                          absl::string_view id) {
  for (const Annotation& ann : annotations) {
    if ((ann.type == Annotation::IDENTIFIER ||
         ann.type == Annotation::FUNCTION_CALL) &&
        ann.id == id) {
      return true;
    }
  }
  return false;
}

// Checks whether `value` lies in `domain` (`fz::Domain::Contains` only checks
// integer domains). Unbounded `float` domains are represented as intervals with
// an empty `float_values` vector.
bool FloatDomainContains(const Domain& domain, double value) {
  if (!domain.is_float) return false;
  if (domain.is_interval) {
    return domain.float_values.empty() ||
           (domain.float_values.size() == 2 &&
            value >= domain.float_values[0] && value <= domain.float_values[1]);
  }
  return domain.float_values.size() == 1 && value == domain.float_values[0];
}

// Formats `token` for syntax diagnostics, surfacing the lexer's message
// directly when `token` is already a lexical error.
std::string DescribeToken(const Token& token) {
  if (token.type == TokenType::kEof) return "end of input";
  if (token.type == TokenType::kError) return token.string_value;
  return absl::StrFormat("%s ('%s')", TokenTypeName(token.type), token.lexeme);
}

// FlatZinc identifiers share a single flat namespace across constants and
// variables. Constants are stored by value (`Domain` represents set literals),
// whereas decision variables are stored as `Variable*` owned by `*model_`.
using SymbolValue =
    std::variant<int64_t, std::vector<int64_t>, double, std::vector<double>,
                 Domain, std::vector<Domain>, Variable*,
                 std::vector<Variable*>>;

// Right-hand side of a variable declaration or element of an `[...]` literal:
// `std::monostate` (unassigned), scalar constant, set literal (`Domain`), or
// existing `Variable*` alias.
using VarRefOrValue =
    std::variant<std::monostate, int64_t, double, Domain, Variable*>;

// Recursive-descent LL(1) parser for FlatZinc (.fzn) models.
// Specification: https://docs.minizinc.org/en/stable/fzn-spec.html#grammar
//
// Enforces the FlatZinc top-level item order:
//   1. `predicate` declarations (`predicate-item*`)
//   2. parameter (constant) and variable declarations
//   3. `constraint` items (`constraint-item*`)
//   4. a single `solve` item (`satisfy`, `minimize`, or `maximize`) + EOF.
class Parser {
 public:
  Parser(absl::string_view input, absl::string_view source_name, Model* model)
      : lexer_(input), source_name_(source_name), model_(model) {
    DCHECK(model_ != nullptr);
  }

  bool ParseModel() {
    using enum TokenType;
    while (PeekType() == kPredicate) {
      if (!ParsePredicate() || !Expect(kSemicolon)) return false;
    }
    while (StartsDeclaration(PeekType())) {
      if (!ParseDeclaration() || !Expect(kSemicolon)) return false;
    }
    while (PeekType() == kConstraint) {
      if (!ParseConstraint() || !Expect(kSemicolon)) return false;
    }
    return ParseSolve() && Expect(kSemicolon) && Expect(kEof);
  }

 private:
  static constexpr int64_t kMaxArraySize = 30'000'000;
  static constexpr int kMaxAnnotationDepth = 128;

  TokenType PeekType() { return lexer_.Peek().type; }

  // Returns true if `type` can begin a parameter or variable declaration.
  // Beyond type keywords, a parameter's domain may start directly with an
  // integer interval (`1..3: k = 2;`), float interval (`0.0..1.0: w = 0.5;`),
  // or integer set literal (`{1, 3}: s = 1;`).
  static bool StartsDeclaration(TokenType type) {
    using enum TokenType;
    switch (type) {
      case kVar:
      case kArray:
      case kBool:
      case kInt:
      case kFloat:
      case kSet:
      case kInteger:
      case kFloatValue:
      case kLBrace:
        return true;
      default:
        return false;
    }
  }

  // Logs the first error encountered at `loc` and suppresses subsequent
  // cascading diagnostics while unwinding recursive-descent frames. Always
  // returns false for convenient tail-call chaining.
  template <typename... Args>
  bool Fail(SourceLocation loc, const absl::FormatSpec<Args...>& format,
            const Args&... args) {
    if (ok_) {
      LOG(ERROR) << source_name_ << ":" << loc.line << ":" << loc.column << ": "
                 << absl::StreamFormat(format, args...);
      ok_ = false;
    }
    return false;
  }

  template <typename... Args>
  bool Fail(const Token& tok, const absl::FormatSpec<Args...>& format,
            const Args&... args) {
    return Fail(tok.loc, format, args...);
  }

  // Reports an `"Expected <expected>, got <tok>"` error at the current
  // lookahead token, preferring the lexer's diagnostic on `TokenType::kError`.
  bool FailExpected(absl::string_view expected) {
    const Token& tok = lexer_.Peek();
    if (tok.type == TokenType::kError) {
      return Fail(tok, "%s", tok.string_value);
    }
    return Fail(tok, "Expected %s, got %s", expected, DescribeToken(tok));
  }

  // Consumes the lookahead token and returns true if it has type `expected`.
  bool Match(TokenType expected) {
    if (lexer_.Peek().type != expected) return false;
    lexer_.Next();
    return true;
  }

  // Consumes a token of type `expected` (storing it in `*out` if non-null), or
  // logs a diagnostic and returns false.
  bool Expect(TokenType expected, Token* out = nullptr) {
    if (lexer_.Peek().type != expected) {
      return FailExpected(TokenTypeName(expected));
    }
    Token consumed = lexer_.Next();
    if (out != nullptr) *out = std::move(consumed);
    return true;
  }

  // Parses one or more comma-separated items (`elem (',' elem)*`). Callers
  // handle empty lists before invoking this helper.
  template <typename Fn>
  bool ParseCommaSeparated(Fn&& parse_elem) {
    do {
      if (!parse_elem()) return false;
    } while (Match(TokenType::kComma));
    return true;
  }

  // FlatZinc arrays are 1-based.
  template <typename T>
  bool Lookup1Based(absl::Span<const T> values, int64_t index,
                    SourceLocation loc, absl::string_view array_name, T* out) {
    if (index < 1 || index > static_cast<int64_t>(values.size())) {
      return Fail(loc, "Index %d out of bounds [1..%d] for array '%s'", index,
                  values.size(), array_name);
    }
    *out = values[index - 1];
    return true;
  }

  // Parses `<int_expr> ']'` after `'['` has been matched.
  bool ParseBracketedIndex(SourceLocation* loc_out, int64_t* idx_out) {
    *loc_out = lexer_.Peek().loc;
    return ParseInteger(idx_out) && Expect(TokenType::kRBracket);
  }

  // Parses `}` or `int_1, ..., int_k }` after `{` has been matched.
  bool ParseBracedIntegersAfterLBrace(std::vector<int64_t>* out) {
    if (Match(TokenType::kRBrace)) return true;
    return ParseIntegerList(out) && Expect(TokenType::kRBrace);
  }

  // Resolves `id` or `id[idx]` expecting scalar `T` or `std::vector<T>`.
  template <typename T>
  bool ParseNamedScalarOrElement(absl::string_view type_label, T* out) {
    const Token id = lexer_.Next();
    const auto it = symbols_.find(id.lexeme);
    if (Match(TokenType::kLBracket)) {
      SourceLocation idx_loc;
      int64_t idx = 0;
      if (!ParseBracketedIndex(&idx_loc, &idx)) return false;
      if (it == symbols_.end()) {
        return Fail(id, "Unknown %s array '%s'", type_label, id.lexeme);
      }
      const auto* arr = std::get_if<std::vector<T>>(&it->second);
      if (arr == nullptr) {
        return Fail(id, "'%s' is not a %s array", id.lexeme, type_label);
      }
      return Lookup1Based<T>(*arr, idx, idx_loc, id.lexeme, out);
    }
    if (it == symbols_.end()) {
      return Fail(id, "Unknown %s constant '%s'", type_label, id.lexeme);
    }
    const auto* val = std::get_if<T>(&it->second);
    if (val == nullptr) {
      return Fail(id, "'%s' is not a %s constant", id.lexeme, type_label);
    }
    *out = *val;
    return true;
  }

  // ---------------------------------------------------------------------------
  // Predicates (parsed for syntactic validity; ignored by fz::Model).
  // https://docs.minizinc.org/en/stable/fzn-spec.html#predicate-declarations
  // Grammar: `predicate <id>(<arg>, ...)` where each `<arg>` has the form
  // `[array [int | lb..ub, ...] of] [var] <domain>: <id>`.
  // ---------------------------------------------------------------------------

  bool ParsePredicate() {
    using enum TokenType;
    if (!Expect(kPredicate) || !Expect(kIdentifier) || !Expect(kLParen)) {
      return false;
    }
    if (Match(kRParen)) return true;
    return ParseCommaSeparated([this] { return ParsePredicateArg(); }) &&
           Expect(kRParen);
  }

  bool ParsePredicateArg() {
    using enum TokenType;
    if (Match(kArray)) {
      if (!Expect(kLBracket)) return false;
      const bool ok = ParseCommaSeparated([this] {
        return Match(kInt) ||
               (Expect(kInteger) && Expect(kDotDot) && Expect(kInteger));
      });
      if (!ok || !Expect(kRBracket) || !Expect(kOf)) return false;
    }
    Match(kVar);
    Domain unused;
    return ParseDomain(&unused) && Expect(kColon) && Expect(kIdentifier);
  }

  // ---------------------------------------------------------------------------
  // Domains and primitive literals/references.
  // https://docs.minizinc.org/en/stable/fzn-spec.html#types
  // ---------------------------------------------------------------------------

  // Parses any parameter or variable domain (`set of ...`, float, or int/bool).
  bool ParseDomain(Domain* out) {
    using enum TokenType;
    if (PeekType() == kSet) return ParseSetDomain(out);
    if (PeekType() == kFloat || PeekType() == kFloatValue) {
      return ParseFloatDomain(out);
    }
    return ParseIntDomain(out);
  }

  // Grammar: `bool | int | <int>..<int> | { <int>, ... }`.
  bool ParseIntDomain(Domain* out) {
    using enum TokenType;
    if (Match(kBool)) {
      *out = Domain::Boolean();
      return true;
    }
    if (Match(kInt)) {
      *out = Domain::AllInt64();
      return true;
    }
    if (PeekType() == kInteger) {
      Token lb;
      Token ub;
      if (!Expect(kInteger, &lb) || !Expect(kDotDot) ||
          !Expect(kInteger, &ub)) {
        return false;
      }
      *out = Domain::Interval(lb.int_value, ub.int_value);
      return true;
    }
    if (Match(kLBrace)) {
      std::vector<int64_t> values;
      if (!ParseBracedIntegersAfterLBrace(&values)) return false;
      *out = values.empty() ? Domain::EmptyDomain()
                            : Domain::IntegerList(std::move(values));
      return true;
    }
    return FailExpected("integer or boolean domain");
  }

  // Grammar: `set of (bool | int | <int>..<int> | { <int>, ... })`.
  bool ParseSetDomain(Domain* out) {
    using enum TokenType;
    if (!Expect(kSet) || !Expect(kOf)) return false;
    if (Match(kBool)) {
      *out = Domain::SetOfBoolean();
      return true;
    }
    if (Match(kInt)) {
      *out = Domain::SetOfAllInt64();
      return true;
    }
    if (PeekType() == kInteger) {
      Token lb;
      Token ub;
      if (!Expect(kInteger, &lb) || !Expect(kDotDot) ||
          !Expect(kInteger, &ub)) {
        return false;
      }
      *out = Domain::SetOfInterval(lb.int_value, ub.int_value);
      return true;
    }
    if (Match(kLBrace)) {
      std::vector<int64_t> values;
      if (!ParseBracedIntegersAfterLBrace(&values)) return false;
      *out = Domain::SetOfIntegerList(std::move(values));
      return true;
    }
    return FailExpected("set element domain");
  }

  // Grammar: `float | <float>..<float>`.
  bool ParseFloatDomain(Domain* out) {
    using enum TokenType;
    if (Match(kFloat)) {
      *out = Domain::AllFloats();
      return true;
    }
    if (PeekType() == kFloatValue) {
      Token lb;
      Token ub;
      if (!Expect(kFloatValue, &lb) || !Expect(kDotDot) ||
          !Expect(kFloatValue, &ub)) {
        return false;
      }
      *out = Domain::FloatInterval(lb.float_value, ub.float_value);
      return true;
    }
    return FailExpected("float domain");
  }

  // Parses an integer literal (including `true`/`false`), named integer
  // constant, or 1-based element of a constant integer array (`arr[idx]`).
  bool ParseInteger(int64_t* out) {
    using enum TokenType;
    if (PeekType() == kInteger) {
      *out = lexer_.Next().int_value;
      return true;
    }
    if (PeekType() == kIdentifier) {
      return ParseNamedScalarOrElement<int64_t>("integer", out);
    }
    return FailExpected("integer value");
  }

  bool ParseIntegerList(std::vector<int64_t>* out) {
    return ParseCommaSeparated([this, out] {
      int64_t v = 0;
      if (!ParseInteger(&v)) return false;
      out->push_back(v);
      return true;
    });
  }

  // Parses a float literal, named float constant, or `float_array[idx]`.
  bool ParseFloat(double* out) {
    using enum TokenType;
    if (PeekType() == kFloatValue) {
      *out = lexer_.Next().float_value;
      return true;
    }
    if (PeekType() == kIdentifier) {
      return ParseNamedScalarOrElement<double>("float", out);
    }
    return FailExpected("float value");
  }

  // Parses `{...}`, `<int>..<int>`, a named set constant, or `set_array[idx]`.
  bool ParseSetLiteral(Domain* out) {
    using enum TokenType;
    if (Match(kLBrace)) {
      std::vector<int64_t> values;
      if (!ParseBracedIntegersAfterLBrace(&values)) return false;
      *out = values.empty() ? Domain::EmptyDomain()
                            : Domain::IntegerList(std::move(values));
      return true;
    }
    if (PeekType() == kInteger) {
      const Token lb = lexer_.Next();
      Token ub;
      if (!Expect(kDotDot) || !Expect(kInteger, &ub)) return false;
      *out = Domain::Interval(lb.int_value, ub.int_value);
      return true;
    }
    if (PeekType() == kIdentifier) {
      return ParseNamedScalarOrElement<Domain>("set", out);
    }
    return FailExpected("set literal");
  }

  // ---------------------------------------------------------------------------
  // Constant and variable declarations.
  // https://docs.minizinc.org/en/stable/fzn-spec.html#parameter-declarations
  // https://docs.minizinc.org/en/stable/fzn-spec.html#variable-declarations
  // ---------------------------------------------------------------------------

  bool ParseDeclaration() {
    using enum TokenType;
    if (PeekType() == kVar) return ParseScalarVarDecl();
    if (PeekType() == kArray) return ParseArrayDecl();
    return ParseScalarConstDecl();
  }

  // Grammar: `<domain>: <id> <annotations> = <const_literal>`.
  bool ParseScalarConstDecl() {
    using enum TokenType;
    const SourceLocation decl_loc = lexer_.Peek().loc;
    Domain domain;
    Token id;
    std::vector<Annotation> annotations;
    if (!ParseDomain(&domain) || !Expect(kColon) || !Expect(kIdentifier, &id) ||
        !ParseAnnotations(&annotations) || !Expect(kEqual)) {
      return false;
    }

    const std::string name(id.lexeme);
    if (domain.is_a_set) {
      Domain set_val;
      if (!ParseSetLiteral(&set_val)) return false;
      symbols_[name] = std::move(set_val);
      return true;
    }
    if (domain.is_float) {
      double float_val = 0.0;
      if (!ParseFloat(&float_val)) return false;
      if (!FloatDomainContains(domain, float_val)) {
        return Fail(decl_loc,
                    "Float constant '%s' is outside its declared domain", name);
      }
      symbols_[name] = float_val;
      return true;
    }

    int64_t int_val = 0;
    if (!ParseInteger(&int_val)) return false;
    if (!domain.Contains(int_val)) {
      return Fail(decl_loc, "Constant '%s' = %d is outside domain %s", name,
                  int_val, domain.DebugString());
    }
    symbols_[name] = int_val;
    return true;
  }

  // Grammar: `var <domain>: <id> <annotations> [= <var_or_value>]`.
  bool ParseScalarVarDecl() {
    using enum TokenType;
    Domain domain;
    Token id;
    std::vector<Annotation> annotations;
    if (!Expect(kVar) || !ParseDomain(&domain) || !Expect(kColon) ||
        !Expect(kIdentifier, &id) || !ParseAnnotations(&annotations)) {
      return false;
    }

    VarRefOrValue rhs;
    SourceLocation rhs_loc = id.loc;
    if (Match(kEqual)) {
      rhs_loc = lexer_.Peek().loc;
      if (!ParseVarOrValue(&rhs)) return false;
    }

    const std::string name(id.lexeme);
    // MiniZinc marks auxiliary variables via `:: var_is_introduced` or the
    // `X_INTRODUCED_` naming prefix.
    const bool introduced =
        ContainsAnnotationId(annotations, "var_is_introduced") ||
        absl::StartsWith(name, "X_INTRODUCED");

    Variable* var = nullptr;
    if (!CreateOrMergeVariable(name, domain, introduced, rhs, rhs_loc, &var)) {
      return false;
    }

    symbols_[name] = var;
    if (ContainsAnnotationId(annotations, "output_var")) {
      model_->AddOutput(SolutionOutputSpecs::SingleVariable(
          name, var, domain.display_as_boolean));
    }
    return true;
  }

  // Grammar: `array [1..n] of (var <var_array_decl> | <const_array_decl>)`.
  // FlatZinc requires 1-based contiguous index sets (`[1..0]` is an empty
  // array).
  bool ParseArrayDecl() {
    using enum TokenType;
    Token lb;
    Token ub;
    if (!Expect(kArray) || !Expect(kLBracket) || !Expect(kInteger, &lb) ||
        !Expect(kDotDot) || !Expect(kInteger, &ub) || !Expect(kRBracket) ||
        !Expect(kOf)) {
      return false;
    }
    if (lb.int_value != 1 || ub.int_value < 0) {
      return Fail(lb,
                  "Only [1..n] arrays with n >= 0 are supported, got [%d..%d]",
                  lb.int_value, ub.int_value);
    }
    if (ub.int_value > kMaxArraySize) {
      return Fail(ub, "Array size %d exceeds maximum supported size (%d)",
                  ub.int_value, kMaxArraySize);
    }
    return Match(kVar) ? ParseVarArrayDecl(ub.int_value)
                       : ParseConstArrayDecl(ub.int_value);
  }

  // Parses the `[e_1, ..., e_n]` initializer (after `[` has been matched) for a
  // constant array of `expected_size` elements and registers it in `symbols_`.
  template <typename T, typename ParseFn>
  bool ParseTypedConstArray(int64_t expected_size, absl::string_view name,
                            SourceLocation loc, ParseFn&& parse_elem) {
    using enum TokenType;
    std::vector<T> values;
    values.reserve(expected_size);
    if (!Match(kRBracket)) {
      const bool ok = ParseCommaSeparated([&] {
        T elem;
        if (!parse_elem(&elem)) return false;
        values.push_back(std::move(elem));
        return true;
      });
      if (!ok || !Expect(kRBracket)) return false;
    }
    if (static_cast<int64_t>(values.size()) != expected_size) {
      return Fail(loc, "Array '%s' declared size %d, got %d items", name,
                  expected_size, values.size());
    }
    symbols_[std::string(name)] = std::move(values);
    return true;
  }

  bool ParseConstArrayDecl(int64_t expected_size) {
    using enum TokenType;
    Domain domain;
    Token id;
    std::vector<Annotation> annotations;
    Token lbracket;
    if (!ParseDomain(&domain) || !Expect(kColon) || !Expect(kIdentifier, &id) ||
        !ParseAnnotations(&annotations) || !Expect(kEqual) ||
        !Expect(kLBracket, &lbracket)) {
      return false;
    }

    if (domain.is_a_set) {
      return ParseTypedConstArray<Domain>(
          expected_size, id.lexeme, lbracket.loc,
          [this](Domain* d) { return ParseSetLiteral(d); });
    }
    if (domain.is_float) {
      return ParseTypedConstArray<double>(
          expected_size, id.lexeme, lbracket.loc, [&](double* v) {
            if (!ParseFloat(v)) return false;
            if (!FloatDomainContains(domain, *v)) {
              return Fail(
                  lbracket,
                  "Element of float array '%s' is outside its declared domain",
                  id.lexeme);
            }
            return true;
          });
    }
    return ParseTypedConstArray<int64_t>(
        expected_size, id.lexeme, lbracket.loc, [&](int64_t* v) {
          if (!ParseInteger(v)) return false;
          if (!domain.Contains(*v)) {
            return Fail(lbracket,
                        "Element %d of array '%s' is outside domain %s", *v,
                        id.lexeme, domain.DebugString());
          }
          return true;
        });
  }

  bool ParseVarArrayDecl(int64_t num_vars) {
    using enum TokenType;
    Domain domain;
    Token id;
    std::vector<Annotation> annotations;
    if (!ParseDomain(&domain) || !Expect(kColon) || !Expect(kIdentifier, &id) ||
        !ParseAnnotations(&annotations)) {
      return false;
    }

    bool has_rhs = false;
    std::vector<VarRefOrValue> assignments;
    SourceLocation rhs_loc = id.loc;
    if (Match(kEqual)) {
      has_rhs = true;
      Token lbracket;
      if (!Expect(kLBracket, &lbracket)) return false;
      rhs_loc = lbracket.loc;
      if (!Match(kRBracket)) {
        if (!ParseVarOrValueList(&assignments) || !Expect(kRBracket)) {
          return false;
        }
      }
      if (static_cast<int64_t>(assignments.size()) != num_vars) {
        return Fail(rhs_loc,
                    "Variable array '%s' declared size %d, got %d items",
                    id.lexeme, num_vars, assignments.size());
      }
    }

    const std::string name(id.lexeme);
    const bool introduced =
        ContainsAnnotationId(annotations, "var_is_introduced") ||
        absl::StartsWith(name, "X_INTRODUCED");

    std::vector<Variable*> vars(num_vars, nullptr);
    for (int64_t i = 0; i < num_vars; ++i) {
      const std::string elem_name = absl::StrFormat("%s[%d]", name, i + 1);
      const VarRefOrValue rhs = has_rhs ? assignments[i] : VarRefOrValue{};
      if (!CreateOrMergeVariable(elem_name, domain, introduced, rhs, rhs_loc,
                                 &vars[i])) {
        return false;
      }
    }

    symbols_[name] = vars;

    // `output_array([lb1..ub1, ...])` records the original MiniZinc
    // multi-dimensional index bounds for solution printing.
    for (const Annotation& ann : annotations) {
      if (!ann.IsFunctionCallWithIdentifier("output_array")) continue;
      if (ann.annotations.size() != 1 ||
          ann.annotations.back().type != Annotation::ANNOTATION_LIST) {
        return Fail(id, "Malformed output_array annotation");
      }
      const Annotation& list = ann.annotations.back();
      std::vector<SolutionOutputSpecs::Bounds> bounds;
      bounds.reserve(list.annotations.size());
      for (const Annotation& bound : list.annotations) {
        if (bound.type != Annotation::INTERVAL) {
          return Fail(id,
                      "Expected interval bounds in output_array annotation");
        }
        bounds.emplace_back(bound.interval_min, bound.interval_max);
      }
      model_->AddOutput(SolutionOutputSpecs::MultiDimensionalArray(
          name, std::move(bounds), vars, domain.display_as_boolean));
    }
    return true;
  }

  // Materializes a declared variable from its `domain` and optional `= rhs`
  // initializer:
  //   - Unassigned (`std::monostate`): creates a fresh variable with `domain`.
  //   - Constant value: creates a variable fixed to that value after checking
  //     domain membership (preserving `display_as_boolean` for `var bool`).
  //   - Existing `Variable*` (`var ...: y = x`): aliases `y` to `x` and
  //     intersects their domains via `Variable::Merge`.
  bool CreateOrMergeVariable(absl::string_view name, const Domain& domain,
                             bool introduced, const VarRefOrValue& rhs,
                             SourceLocation loc, Variable** out) {
    return std::visit(
        absl::Overload{
            [&](std::monostate) {
              *out = model_->AddVariable(name, domain, introduced);
              return true;
            },
            [&](double v) {
              if (!domain.is_float || !FloatDomainContains(domain, v)) {
                return Fail(loc, "Incompatible float assignment for '%s'",
                            name);
              }
              *out =
                  model_->AddVariable(name, Domain::FloatValue(v), introduced);
              return true;
            },
            [&](const Domain& d) {
              if (!domain.is_a_set) {
                return Fail(
                    loc, "Cannot assign set literal to non-set variable '%s'",
                    name);
              }
              *out = model_->AddVariable(name, d, introduced,
                                         /*set_is_fixed=*/true);
              return true;
            },
            [&](int64_t v) {
              if (domain.is_float || domain.is_a_set || !domain.Contains(v)) {
                return Fail(loc, "Value %d for '%s' is outside domain %s", v,
                            name, domain.DebugString());
              }
              Domain fixed = Domain::IntegerValue(v);
              fixed.display_as_boolean = domain.display_as_boolean;
              *out = model_->AddVariable(name, fixed, introduced);
              return true;
            },
            [&](Variable* var) {
              if (var->domain.is_float != domain.is_float ||
                  var->domain.is_a_set != domain.is_a_set) {
                return Fail(loc, "Incompatible variable alias for '%s'", name);
              }
              *out = var;
              (*out)->Merge(name, domain, introduced);
              return true;
            },
        },
        rhs);
  }

  // Parses a scalar literal, set literal (`lb..ub` or `{...}`), scalar symbol,
  // or `array_symbol[idx]` element.
  bool ParseVarOrValue(VarRefOrValue* out) {
    using enum TokenType;
    if (PeekType() == kInteger) {
      const Token first = lexer_.Next();
      if (Match(kDotDot)) {
        Token second;
        if (!Expect(kInteger, &second)) return false;
        *out = Domain::Interval(first.int_value, second.int_value);
        return true;
      }
      *out = first.int_value;
      return true;
    }
    if (PeekType() == kFloatValue) {
      *out = lexer_.Next().float_value;
      return true;
    }
    if (Match(kLBrace)) {
      std::vector<int64_t> values;
      if (!ParseBracedIntegersAfterLBrace(&values)) return false;
      *out = values.empty() ? Domain::EmptyDomain()
                            : Domain::IntegerList(std::move(values));
      return true;
    }
    if (PeekType() == kIdentifier) {
      const Token id = lexer_.Next();
      const auto it = symbols_.find(id.lexeme);
      if (Match(kLBracket)) {
        SourceLocation idx_loc;
        int64_t idx = 0;
        if (!ParseBracketedIndex(&idx_loc, &idx)) return false;
        if (it == symbols_.end()) {
          return Fail(id, "Unknown array symbol '%s'", id.lexeme);
        }
        return std::visit(
            absl::Overload{
                [&]<typename T>(const std::vector<T>& arr) {
                  T elem;
                  if (!Lookup1Based<T>(arr, idx, idx_loc, id.lexeme, &elem)) {
                    return false;
                  }
                  *out = std::move(elem);
                  return true;
                },
                [&](const auto&) {
                  return Fail(id, "'%s' is not an array", id.lexeme);
                },
            },
            it->second);
      }
      if (it == symbols_.end()) {
        return Fail(id, "Unknown symbol '%s'", id.lexeme);
      }
      return std::visit(absl::Overload{
                            [&]<typename T>(const std::vector<T>&) {
                              return Fail(id, "Expected scalar, got array '%s'",
                                          id.lexeme);
                            },
                            [&](const auto& scalar) {
                              *out = scalar;
                              return true;
                            },
                        },
                        it->second);
    }
    return FailExpected("variable or value");
  }

  bool ParseVarOrValueList(std::vector<VarRefOrValue>* out) {
    return ParseCommaSeparated([this, out] {
      VarRefOrValue item;
      if (!ParseVarOrValue(&item)) return false;
      out->push_back(std::move(item));
      return true;
    });
  }

  // ---------------------------------------------------------------------------
  // Constraints and arguments.
  // https://docs.minizinc.org/en/stable/fzn-spec.html#constraints
  // ---------------------------------------------------------------------------

  // Grammar: `constraint <id>(<arg>, ...) <annotations>`.
  bool ParseConstraint() {
    using enum TokenType;
    Token id;
    if (!Expect(kConstraint) || !Expect(kIdentifier, &id) || !Expect(kLParen)) {
      return false;
    }
    std::vector<Argument> arguments;
    if (!Match(kRParen)) {
      const bool ok = ParseCommaSeparated([&] {
        Argument arg;
        if (!ParseArgument(&arg)) return false;
        arguments.push_back(std::move(arg));
        return true;
      });
      if (!ok || !Expect(kRParen)) return false;
    }

    std::vector<Annotation> annotations;
    if (!ParseAnnotations(&annotations)) return false;

    model_->AddConstraint(
        id.lexeme, std::move(arguments),
        ContainsAnnotationId(annotations, "domain"),
        ContainsAnnotationId(annotations, "symmetry_breaking"),
        ContainsAnnotationId(annotations, "redundant"));
    return true;
  }

  bool ParseArgument(Argument* out) {
    using enum TokenType;
    if (PeekType() == kInteger) {
      const Token first = lexer_.Next();
      if (Match(kDotDot)) {
        Token second;
        if (!Expect(kInteger, &second)) return false;
        *out = Argument::Interval(first.int_value, second.int_value);
        return true;
      }
      *out = Argument::IntegerValue(first.int_value);
      return true;
    }
    if (PeekType() == kFloatValue) {
      const Token first = lexer_.Next();
      if (Match(kDotDot)) {
        Token second;
        if (!Expect(kFloatValue, &second)) return false;
        *out = Argument::FloatInterval(first.float_value, second.float_value);
        return true;
      }
      *out = Argument::FloatValue(first.float_value);
      return true;
    }
    if (PeekType() == kString) {
      // String constraint arguments carry no solver semantics in `fz::Model`.
      lexer_.Next();
      *out = Argument::VoidArgument();
      return true;
    }
    if (Match(kLBrace)) {
      std::vector<int64_t> values;
      if (!ParseBracedIntegersAfterLBrace(&values)) return false;
      *out = Argument::IntegerList(std::move(values));
      return true;
    }
    if (PeekType() == kIdentifier) return ParseIdentifierArgument(out);
    if (PeekType() == kLBracket) return ParseArrayLiteralArgument(out);
    return FailExpected("constraint argument");
  }

  // Resolves a named constant, variable, array, or `array[idx]` element into a
  // constraint `Argument`.
  bool ParseIdentifierArgument(Argument* out) {
    using enum TokenType;
    const Token id = lexer_.Next();
    const auto it = symbols_.find(id.lexeme);
    if (Match(kLBracket)) {
      SourceLocation idx_loc;
      int64_t idx = 0;
      if (!ParseBracketedIndex(&idx_loc, &idx)) return false;
      if (it == symbols_.end()) {
        return Fail(id, "Unknown array identifier '%s'", id.lexeme);
      }
      auto lookup = [&]<typename T>(const std::vector<T>& arr, T* elem) {
        return Lookup1Based<T>(arr, idx, idx_loc, id.lexeme, elem);
      };
      return std::visit(absl::Overload{
                            [&](const std::vector<int64_t>& arr) {
                              int64_t v = 0;
                              if (!lookup(arr, &v)) return false;
                              *out = Argument::IntegerValue(v);
                              return true;
                            },
                            [&](const std::vector<double>& arr) {
                              double v = 0.0;
                              if (!lookup(arr, &v)) return false;
                              *out = Argument::FloatValue(v);
                              return true;
                            },
                            [&](const std::vector<Variable*>& arr) {
                              Variable* v = nullptr;
                              if (!lookup(arr, &v)) return false;
                              *out = Argument::VarRef(v);
                              return true;
                            },
                            [&](const std::vector<Domain>& arr) {
                              Domain d;
                              if (!lookup(arr, &d)) return false;
                              *out = Argument::FromDomain(d);
                              return true;
                            },
                            [&](const auto&) {
                              return Fail(id, "'%s' is not an array",
                                          id.lexeme);
                            },
                        },
                        it->second);
    }

    if (it == symbols_.end()) {
      return Fail(id, "Unknown identifier '%s'", id.lexeme);
    }
    *out = std::visit(
        absl::Overload{
            [](int64_t v) { return Argument::IntegerValue(v); },
            [](const std::vector<int64_t>& v) {
              return Argument::IntegerList(v);
            },
            [](double v) { return Argument::FloatValue(v); },
            [](const std::vector<double>& v) { return Argument::FloatList(v); },
            [](const Domain& d) { return Argument::FromDomain(d); },
            [](const std::vector<Domain>& d) {
              return Argument::DomainList(d);
            },
            [](Variable* v) { return Argument::VarRef(v); },
            [](const std::vector<Variable*>& v) {
              return Argument::VarRefArray(v);
            },
        },
        it->second);
    return true;
  }

  // Parses an `[e_1, ..., e_n]` constraint argument and promotes mixed
  // literal/variable arrays to a uniform `Argument` representation:
  //   - `[]` -> `VoidArgument`.
  //   - Any scalar `Variable*` (optionally mixed with int/float constants) ->
  //     `VarRefArray`, materializing constants via `Model::Add*Constant`.
  //   - Any set `Variable*` (optionally mixed with set literals) ->
  //     `VarRefArray`, materializing set literals via `Model::AddSetConstant`.
  //   - Pure constants -> `DomainList`, `FloatList`, or `IntegerList`.
  bool ParseArrayLiteralArgument(Argument* out) {
    using enum TokenType;
    Token lbracket;
    if (!Expect(kLBracket, &lbracket)) return false;
    if (Match(kRBracket)) {
      *out = Argument::VoidArgument();
      return true;
    }

    std::vector<VarRefOrValue> elems;
    if (!ParseVarOrValueList(&elems) || !Expect(kRBracket)) return false;

    bool has_scalar_vars = false;
    bool has_set_vars = false;
    bool has_floats = false;
    bool has_domains = false;
    bool has_ints = false;
    for (const VarRefOrValue& elem : elems) {
      std::visit(absl::Overload{
                     [](std::monostate) {},
                     [&](int64_t) { has_ints = true; },
                     [&](double) { has_floats = true; },
                     [&](const Domain&) { has_domains = true; },
                     [&](Variable* v) {
                       if (v->domain.is_a_set) {
                         has_set_vars = true;
                       } else {
                         has_scalar_vars = true;
                       }
                     },
                 },
                 elem);
    }

    if (has_scalar_vars) {
      if (has_set_vars || has_domains) {
        return Fail(lbracket,
                    "Cannot mix scalar variables and sets in array argument");
      }
      std::vector<Variable*> vars;
      vars.reserve(elems.size());
      for (const VarRefOrValue& elem : elems) {
        if (const auto* v = std::get_if<Variable*>(&elem)) {
          vars.push_back(*v);
        } else if (const auto* f = std::get_if<double>(&elem)) {
          vars.push_back(model_->AddFloatConstant(*f));
        } else {
          vars.push_back(model_->AddConstant(std::get<int64_t>(elem)));
        }
      }
      *out = Argument::VarRefArray(std::move(vars));
      return true;
    }

    if (has_set_vars) {
      if (has_floats || has_ints) {
        return Fail(lbracket,
                    "Cannot mix set variables and scalars in array argument");
      }
      std::vector<Variable*> vars;
      vars.reserve(elems.size());
      for (const VarRefOrValue& elem : elems) {
        if (const auto* v = std::get_if<Variable*>(&elem)) {
          vars.push_back(*v);
        } else {
          vars.push_back(model_->AddSetConstant(std::get<Domain>(elem)));
        }
      }
      *out = Argument::VarRefArray(std::move(vars));
      return true;
    }

    if (has_domains) {
      if (has_floats || has_ints) {
        return Fail(lbracket,
                    "Cannot mix set literals and scalars in array argument");
      }
      std::vector<Domain> domains;
      domains.reserve(elems.size());
      for (VarRefOrValue& elem : elems) {
        domains.push_back(std::move(std::get<Domain>(elem)));
      }
      *out = Argument::DomainList(std::move(domains));
      return true;
    }

    if (has_floats) {
      std::vector<double> values;
      values.reserve(elems.size());
      for (const VarRefOrValue& elem : elems) {
        const auto* f = std::get_if<double>(&elem);
        values.push_back(
            f != nullptr ? *f : static_cast<double>(std::get<int64_t>(elem)));
      }
      *out = Argument::FloatList(std::move(values));
      return true;
    }

    std::vector<int64_t> values;
    values.reserve(elems.size());
    for (const VarRefOrValue& elem : elems) {
      values.push_back(std::get<int64_t>(elem));
    }
    *out = Argument::IntegerList(std::move(values));
    return true;
  }

  // ---------------------------------------------------------------------------
  // Annotations (`('::' <annotation>)*`).
  // https://docs.minizinc.org/en/stable/fzn-spec.html#annotations
  // ---------------------------------------------------------------------------

  bool ParseAnnotations(std::vector<Annotation>* out) {
    while (Match(TokenType::kColonColon)) {
      Annotation ann;
      if (!ParseAnnotation(&ann, /*depth=*/0)) return false;
      out->push_back(std::move(ann));
    }
    return true;
  }

  bool ParseAnnotationList(std::vector<Annotation>* out, int depth) {
    return ParseCommaSeparated([this, out, depth] {
      Annotation ann;
      if (!ParseAnnotation(&ann, depth)) return false;
      out->push_back(std::move(ann));
      return true;
    });
  }

  bool ParseAnnotation(Annotation* out, int depth) {
    using enum TokenType;
    if (depth > kMaxAnnotationDepth) {
      return Fail(lexer_.Peek(),
                  "Annotation nesting depth exceeds maximum (%d)",
                  kMaxAnnotationDepth);
    }
    if (PeekType() == kInteger) {
      const Token first = lexer_.Next();
      if (Match(kDotDot)) {
        Token second;
        if (!Expect(kInteger, &second)) return false;
        *out = Annotation::Interval(first.int_value, second.int_value);
        return true;
      }
      *out = Annotation::IntegerValue(first.int_value);
      return true;
    }
    if (PeekType() == kFloatValue) {
      const Token first = lexer_.Next();
      if (Match(kDotDot)) {
        Token second;
        if (!Expect(kFloatValue, &second)) return false;
        *out = Annotation::Identifier(
            absl::StrFormat("%s..%s", first.lexeme, second.lexeme));
        return true;
      }
      *out = Annotation::Identifier(first.lexeme);
      return true;
    }
    if (Match(kLBrace)) {
      std::vector<int64_t> values;
      if (!ParseBracedIntegersAfterLBrace(&values)) return false;
      *out = Annotation::IntegerList(values);
      return true;
    }
    if (PeekType() == kString) {
      *out = Annotation::String(lexer_.Next().string_value);
      return true;
    }
    if (PeekType() == kIdentifier) {
      return ParseIdentifierAnnotation(out, depth);
    }
    if (Match(kLBracket)) {
      if (Match(kRBracket)) {
        *out = Annotation::Empty();
        return true;
      }
      std::vector<Annotation> elems;
      if (!ParseAnnotationList(&elems, depth + 1) || !Expect(kRBracket)) {
        return false;
      }

      // Search annotations such as `int_search([x1, x2], ...)` expect
      // homogeneous integer or variable lists to be collapsed into `INT_LIST`
      // or `VAR_REF_ARRAY` rather than a generic `ANNOTATION_LIST`.
      bool all_ints = true;
      bool all_vars = true;
      for (const Annotation& ann : elems) {
        if (ann.type != Annotation::INT_VALUE) all_ints = false;
        if (ann.type != Annotation::VAR_REF) all_vars = false;
      }
      if (all_ints) {
        std::vector<int64_t> values;
        values.reserve(elems.size());
        for (const Annotation& ann : elems) values.push_back(ann.interval_min);
        *out = Annotation::IntegerList(values);
      } else if (all_vars) {
        std::vector<Variable*> vars;
        vars.reserve(elems.size());
        for (const Annotation& ann : elems) vars.push_back(ann.variables[0]);
        *out = Annotation::VarRefArray(std::move(vars));
      } else {
        *out = Annotation::AnnotationList(std::move(elems));
      }
      return true;
    }
    return FailExpected("annotation");
  }

  // Parses `id(...)`, `id[idx]`, or bare `id` inside an annotation. Unlike
  // constraint arguments, unknown bare identifiers are valid symbolic atoms in
  // FlatZinc (e.g., `input_order`, `indomain_min`, `complete`, `output_var`).
  bool ParseIdentifierAnnotation(Annotation* out, int depth) {
    using enum TokenType;
    const Token id = lexer_.Next();
    if (Match(kLParen)) {
      if (Match(kRParen)) {
        *out = Annotation::FunctionCall(id.lexeme);
        return true;
      }
      std::vector<Annotation> args;
      if (!ParseAnnotationList(&args, depth + 1) || !Expect(kRParen)) {
        return false;
      }
      *out = Annotation::FunctionCallWithArguments(id.lexeme, std::move(args));
      return true;
    }

    const auto it = symbols_.find(id.lexeme);
    if (Match(kLBracket)) {
      SourceLocation idx_loc;
      int64_t idx = 0;
      if (!ParseBracketedIndex(&idx_loc, &idx)) return false;
      if (it == symbols_.end()) {
        return Fail(id, "Unknown array identifier in annotation: '%s'",
                    id.lexeme);
      }
      return std::visit(
          absl::Overload{
              [&](const std::vector<Variable*>& arr) {
                Variable* var = nullptr;
                if (!Lookup1Based<Variable*>(arr, idx, idx_loc, id.lexeme,
                                             &var)) {
                  return false;
                }
                *out = Annotation::VarRef(var);
                return true;
              },
              [&](const std::vector<int64_t>& arr) {
                int64_t v = 0;
                if (!Lookup1Based<int64_t>(arr, idx, idx_loc, id.lexeme, &v)) {
                  return false;
                }
                *out = Annotation::IntegerValue(v);
                return true;
              },
              [&](const auto&) {
                return Fail(id, "Unsupported array '%s' in annotation",
                            id.lexeme);
              },
          },
          it->second);
    }

    if (it == symbols_.end()) {
      *out = Annotation::Identifier(id.lexeme);
      return true;
    }
    *out = std::visit(
        absl::Overload{
            [](Variable* v) { return Annotation::VarRef(v); },
            [](const std::vector<Variable*>& v) {
              return Annotation::VarRefArray(v);
            },
            [](int64_t v) { return Annotation::IntegerValue(v); },
            [](const std::vector<int64_t>& v) {
              return Annotation::IntegerList(v);
            },
            [&](const auto&) { return Annotation::Identifier(id.lexeme); },
        },
        it->second);
    return true;
  }

  // ---------------------------------------------------------------------------
  // Solve item (`solve <annotations> (satisfy | minimize <e> | maximize <e>)`).
  // https://docs.minizinc.org/en/stable/fzn-spec.html#solve-item
  // ---------------------------------------------------------------------------

  bool ParseSolve() {
    using enum TokenType;
    std::vector<Annotation> annotations;
    if (!Expect(kSolve) || !ParseAnnotations(&annotations)) return false;

    if (Match(kSatisfy)) {
      model_->Satisfy(std::move(annotations));
      return true;
    }

    const bool minimize = Match(kMinimize);
    if (!minimize && !Match(kMaximize)) {
      return FailExpected("'satisfy', 'minimize', or 'maximize' in solve item");
    }

    const SourceLocation obj_loc = lexer_.Peek().loc;
    Argument obj_arg;
    if (!ParseArgument(&obj_arg)) return false;

    // MiniZinc presolve may simplify an objective to a scalar constant; wrap it
    // as a constant `Variable*` in `*model_` so `Minimize`/`Maximize` stay
    // uniform.
    Variable* obj_var = nullptr;
    if (obj_arg.type == Argument::VAR_REF) {
      obj_var = obj_arg.Var();
    } else if (obj_arg.type == Argument::INT_VALUE) {
      obj_var = model_->AddConstant(obj_arg.Value());
    } else if (obj_arg.type == Argument::FLOAT_VALUE) {
      obj_var = model_->AddFloatConstant(obj_arg.floats[0]);
    } else {
      return Fail(obj_loc,
                  "Objective must be a scalar variable or constant value");
    }

    if (minimize) {
      model_->Minimize(obj_var, std::move(annotations));
    } else {
      model_->Maximize(obj_var, std::move(annotations));
    }
    return true;
  }

  Lexer lexer_;
  absl::string_view source_name_;  // Filename or `"<string>"` for diagnostics.
  Model* model_;
  absl::flat_hash_map<std::string, SymbolValue> symbols_;
  bool ok_ = true;  // False once the first diagnostic has been logged.
};

bool ParseFlatzincImpl(absl::string_view input, absl::string_view source_name,
                       Model* model) {
  return Parser(input, source_name, model).ParseModel();
}

}  // namespace

bool ParseFlatzincFile(absl::string_view filename, Model* model) {
  std::string contents;
  const absl::Status status =
      file::GetContents(filename, &contents, file::Defaults());
  if (!status.ok()) {
    LOG(ERROR) << "Could not open file '" << filename << "': " << status;
    return false;
  }
  return ParseFlatzincImpl(contents, filename, model);
}

bool ParseFlatzincString(absl::string_view input, Model* model) {
  return ParseFlatzincImpl(input, "<string>", model);
}

}  // namespace operations_research::fz
