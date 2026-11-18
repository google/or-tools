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

#include "ortools/flatzinc/lexer.h"

#include <array>
#include <charconv>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <utility>

#include "absl/strings/ascii.h"
#include "absl/strings/numbers.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/strings/string_view.h"

namespace operations_research::fz {
namespace {

bool IsIdentContinue(char ch) { return absl::ascii_isalnum(ch) || ch == '_'; }

constexpr auto kKeywords =
    std::to_array<std::pair<absl::string_view, TokenType>>({
        {"array", TokenType::kArray},
        {"bool", TokenType::kBool},
        {"constraint", TokenType::kConstraint},
        {"float", TokenType::kFloat},
        {"int", TokenType::kInt},
        {"maximize", TokenType::kMaximize},
        {"minimize", TokenType::kMinimize},
        {"of", TokenType::kOf},
        {"predicate", TokenType::kPredicate},
        {"satisfy", TokenType::kSatisfy},
        {"set", TokenType::kSet},
        {"solve", TokenType::kSolve},
        {"var", TokenType::kVar},
    });

std::optional<TokenType> SingleCharPunctuation(char ch) {
  using enum TokenType;
  switch (ch) {
    case ';':
      return kSemicolon;
    case ',':
      return kComma;
    case '(':
      return kLParen;
    case ')':
      return kRParen;
    case '[':
      return kLBracket;
    case ']':
      return kRBracket;
    case '{':
      return kLBrace;
    case '}':
      return kRBrace;
    case '=':
      return kEqual;
    default:
      return std::nullopt;
  }
}

}  // namespace

absl::string_view TokenTypeName(TokenType type) {
  using enum TokenType;
  switch (type) {
    case kEof:
      return "end of input";
    case kError:
      return "lexical error";
    case kSemicolon:
      return "';'";
    case kColon:
      return "':'";
    case kColonColon:
      return "'::'";
    case kComma:
      return "','";
    case kDotDot:
      return "'..'";
    case kLParen:
      return "'('";
    case kRParen:
      return "')'";
    case kLBracket:
      return "'['";
    case kRBracket:
      return "']'";
    case kLBrace:
      return "'{'";
    case kRBrace:
      return "'}'";
    case kEqual:
      return "'='";
    case kArray:
      return "'array'";
    case kBool:
      return "'bool'";
    case kConstraint:
      return "'constraint'";
    case kFloat:
      return "'float'";
    case kInt:
      return "'int'";
    case kMaximize:
      return "'maximize'";
    case kMinimize:
      return "'minimize'";
    case kOf:
      return "'of'";
    case kPredicate:
      return "'predicate'";
    case kSatisfy:
      return "'satisfy'";
    case kSet:
      return "'set'";
    case kSolve:
      return "'solve'";
    case kVar:
      return "'var'";
    case kInteger:
      return "integer literal";
    case kFloatValue:
      return "float literal";
    case kString:
      return "string literal";
    case kIdentifier:
      return "identifier";
  }
  return "unknown token";
}

const Token& Lexer::Peek() {
  if (!peeked_.has_value()) {
    peeked_ = ScanNextToken();
  }
  return *peeked_;
}

Token Lexer::Next() {
  if (peeked_.has_value()) {
    return *std::exchange(peeked_, std::nullopt);
  }
  return ScanNextToken();
}

char Lexer::PeekChar(size_t offset) const {
  const size_t idx = cursor_.offset + offset;
  return idx < input_.size() ? input_[idx] : '\0';
}

char Lexer::AdvanceChar() {
  const char ch = input_[cursor_.offset++];
  if (ch == '\n') {
    ++cursor_.loc.line;
    cursor_.loc.column = 1;
  } else {
    ++cursor_.loc.column;
  }
  return ch;
}

template <typename Pred>
void Lexer::AdvanceWhile(Pred&& pred) {
  while (!AtEnd() && pred(input_[cursor_.offset])) {
    AdvanceChar();
  }
}

Token Lexer::MakeToken(TokenType type) const {
  return {.type = type, .loc = token_start_.loc, .lexeme = CurrentLexeme()};
}

Token Lexer::MakeError(std::string message) const {
  return {.type = TokenType::kError,
          .loc = token_start_.loc,
          .lexeme = CurrentLexeme(),
          .string_value = std::move(message)};
}

// https://docs.minizinc.org/en/stable/fzn-spec.html#comments
void Lexer::SkipWhitespaceAndComments() {
  while (!AtEnd()) {
    const char ch = input_[cursor_.offset];
    if (ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n') {
      AdvanceChar();
    } else if (ch == '%') {
      // FlatZinc line comment: extends to the end of the line.
      AdvanceWhile([](char c) { return c != '\n'; });
    } else {
      break;
    }
  }
}

Token Lexer::ScanNextToken() {
  using enum TokenType;
  SkipWhitespaceAndComments();
  token_start_ = cursor_;
  if (AtEnd()) {
    return MakeToken(kEof);
  }

  const char ch = PeekChar();
  if (const std::optional<TokenType> punct = SingleCharPunctuation(ch);
      punct.has_value()) {
    AdvanceChar();
    return MakeToken(*punct);
  }
  if (ch == ':') {
    AdvanceChar();
    if (PeekChar() == ':') {
      AdvanceChar();
      return MakeToken(kColonColon);
    }
    return MakeToken(kColon);
  }
  if (ch == '.') {
    AdvanceChar();
    if (PeekChar() == '.') {
      AdvanceChar();
      return MakeToken(kDotDot);
    }
    return MakeError("Unexpected '.' (expected '..' interval operator)");
  }
  if (ch == '"') {
    return ScanString();
  }
  if (absl::ascii_isalpha(ch) || ch == '_') {
    return ScanIdentifierOrKeyword();
  }
  if (absl::ascii_isdigit(ch) ||
      ((ch == '-' || ch == '+') && absl::ascii_isdigit(PeekChar(1)))) {
    return ScanNumber();
  }

  AdvanceChar();
  if (absl::ascii_isprint(ch)) {
    return MakeError(absl::StrFormat("Unexpected character '%c'", ch));
  }
  return MakeError(absl::StrFormat("Unexpected byte 0x%02x",
                                   static_cast<unsigned char>(ch)));
}

// https://docs.minizinc.org/en/stable/fzn-spec.html#flatzinc-models
Token Lexer::ScanIdentifierOrKeyword() {
  if (PeekChar() == '_') {
    AdvanceWhile([](char c) { return c == '_'; });
    // FlatZinc identifiers starting with '_' must contain at least one letter
    // immediately after the leading underscores: _+[A-Za-z][A-Za-z0-9_]*.
    if (!absl::ascii_isalpha(PeekChar())) {
      AdvanceWhile(IsIdentContinue);
      return MakeError(
          absl::StrCat("Invalid identifier '", CurrentLexeme(), "'"));
    }
  }

  AdvanceWhile(IsIdentContinue);

  const absl::string_view lexeme = CurrentLexeme();
  for (const auto& [kw, type] : kKeywords) {
    if (lexeme == kw) return MakeToken(type);
  }
  if (lexeme == "true" || lexeme == "false") {
    return {.type = TokenType::kInteger,
            .loc = token_start_.loc,
            .lexeme = lexeme,
            .int_value = (lexeme == "true") ? 1 : 0};
  }
  return MakeToken(TokenType::kIdentifier);
}

// Parses a `0x...` (base 16) or `0o...` (base 8) integer literal after an
// optional sign has been consumed. Magnitude is parsed into `uint64_t` first so
// `INT64_MIN` (`-0x8000000000000000`) can be represented without overflow.
Token Lexer::ScanRadixInteger(bool negative, int base) {
  AdvanceChar();  // '0'
  AdvanceChar();  // 'x'/'X' or 'o'/'O'
  const size_t digits_start = cursor_.offset;
  AdvanceWhile(IsIdentContinue);
  const absl::string_view digits =
      input_.substr(digits_start, cursor_.offset - digits_start);

  uint64_t mag = 0;
  const auto [ptr, ec] =
      std::from_chars(digits.data(), digits.data() + digits.size(), mag, base);
  const uint64_t max_mag =
      static_cast<uint64_t>(std::numeric_limits<int64_t>::max()) +
      (negative ? 1ULL : 0ULL);
  // Value-initialized `ec` indicates success; spelled via `decltype` because
  // naming `std::errc` requires `<system_error>`, which cpplint disallows.
  if (digits.empty() || ptr != digits.data() + digits.size() ||
      ec != decltype(ec){} || mag > max_mag) {
    return MakeError(absl::StrCat("Invalid base-", base, " integer literal '",
                                  CurrentLexeme(), "'"));
  }
  return {.type = TokenType::kInteger,
          .loc = token_start_.loc,
          .lexeme = CurrentLexeme(),
          .int_value = static_cast<int64_t>(negative ? 0ULL - mag : mag)};
}

// https://docs.minizinc.org/en/stable/fzn-spec.html#grammar
Token Lexer::ScanNumber() {
  bool negative = false;
  if (PeekChar() == '-' || PeekChar() == '+') {
    negative = (AdvanceChar() == '-');
  }

  if (PeekChar() == '0' && (PeekChar(1) == 'x' || PeekChar(1) == 'X')) {
    return ScanRadixInteger(negative, /*base=*/16);
  }
  if (PeekChar() == '0' && (PeekChar(1) == 'o' || PeekChar(1) == 'O')) {
    return ScanRadixInteger(negative, /*base=*/8);
  }

  AdvanceWhile(absl::ascii_isdigit);

  bool is_float = false;
  // Guard against `..` so intervals like `1..10` tokenize as `1`, `..`, `10`
  // rather than a malformed float literal `1.`.
  if (PeekChar() == '.' && PeekChar(1) != '.') {
    if (!absl::ascii_isdigit(PeekChar(1))) {
      AdvanceChar();  // Consume stray '.' for error span.
      return MakeError("Malformed float literal: expected digits after '.'");
    }
    is_float = true;
    AdvanceChar();  // '.'
    AdvanceWhile(absl::ascii_isdigit);
  }

  if (PeekChar() == 'e' || PeekChar() == 'E') {
    is_float = true;
    AdvanceChar();  // 'e' or 'E'
    if (PeekChar() == '+' || PeekChar() == '-') {
      AdvanceChar();
    }
    if (!absl::ascii_isdigit(PeekChar())) {
      return MakeError(
          "Malformed float literal: expected exponent digits after 'e'");
    }
    AdvanceWhile(absl::ascii_isdigit);
  }

  if (IsIdentContinue(PeekChar())) {
    AdvanceWhile(IsIdentContinue);
    return MakeError(
        absl::StrCat("Invalid numeric literal '", CurrentLexeme(), "'"));
  }

  const absl::string_view lexeme = CurrentLexeme();
  if (is_float) {
    double value = 0.0;
    if (!absl::SimpleAtod(lexeme, &value) || !std::isfinite(value)) {
      return MakeError(
          absl::StrCat("Float literal out of range: '", lexeme, "'"));
    }
    return {.type = TokenType::kFloatValue,
            .loc = token_start_.loc,
            .lexeme = lexeme,
            .float_value = value};
  }

  int64_t value = 0;
  if (!absl::SimpleAtoi(lexeme, &value)) {
    return MakeError(
        absl::StrCat("Integer literal out of int64 range: '", lexeme, "'"));
  }
  return {.type = TokenType::kInteger,
          .loc = token_start_.loc,
          .lexeme = lexeme,
          .int_value = value};
}

// https://docs.minizinc.org/en/stable/fzn-spec.html#the-string-type
Token Lexer::ScanString() {
  AdvanceChar();  // Opening '"'
  std::string decoded;
  while (!AtEnd()) {
    const char ch = PeekChar();
    if (ch == '"') {
      AdvanceChar();  // Closing '"'
      return {.type = TokenType::kString,
              .loc = token_start_.loc,
              .lexeme = CurrentLexeme(),
              .string_value = std::move(decoded)};
    }
    if (ch == '\n' || ch == '\r') {
      return MakeError("Unterminated string literal");
    }
    AdvanceChar();
    if (ch != '\\') {
      decoded.push_back(ch);
      continue;
    }
    if (AtEnd() || PeekChar() == '\n' || PeekChar() == '\r') {
      return MakeError("Unterminated escape sequence in string literal");
    }
    switch (const char esc = AdvanceChar()) {
      case '"':
      case '\'':
      case '\\':
        decoded.push_back(esc);
        break;
      case 'n':
        decoded.push_back('\n');
        break;
      case 'r':
        decoded.push_back('\r');
        break;
      case 't':
        decoded.push_back('\t');
        break;
      default:
        return MakeError(absl::StrFormat(
            "Unsupported escape sequence '\\%c' in string", esc));
    }
  }
  return MakeError("Unterminated string literal");
}

}  // namespace operations_research::fz
