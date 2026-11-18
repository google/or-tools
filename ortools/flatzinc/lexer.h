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

#ifndef ORTOOLS_FLATZINC_LEXER_H_
#define ORTOOLS_FLATZINC_LEXER_H_

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>

#include "absl/strings/string_view.h"

namespace operations_research::fz {

// 1-based line and byte-column coordinates within the FlatZinc input buffer.
struct SourceLocation {
  int line = 1;
  int column = 1;

  friend bool operator==(const SourceLocation&,
                         const SourceLocation&) = default;
};

enum class TokenType {
  // End of input and lexical error sentinel.
  kEof,
  kError,

  // Punctuation.
  kSemicolon,   // ;
  kColon,       // :
  kColonColon,  // ::
  kComma,       // ,
  kDotDot,      // ..
  kLParen,      // (
  kRParen,      // )
  kLBracket,    // [
  kRBracket,    // ]
  kLBrace,      // {
  kRBrace,      // }
  kEqual,       // =

  // Keywords.
  kArray,       // array
  kBool,        // bool
  kConstraint,  // constraint
  kFloat,       // float
  kInt,         // int
  kMaximize,    // maximize
  kMinimize,    // minimize
  kOf,          // of
  kPredicate,   // predicate
  kSatisfy,     // satisfy
  kSet,         // set
  kSolve,       // solve
  kVar,         // var

  // Literals and identifiers.
  // Note: FlatZinc boolean literals ("true" and "false") are represented in
  // fz::Model as integer values 1 and 0, so the lexer emits kInteger for them.
  kInteger,     // int64_t literal (decimal, 0x hex, 0o octal, or true/false)
  kFloatValue,  // double literal
  kString,      // unquoted, unescaped string literal
  kIdentifier,  // [A-Za-z][A-Za-z0-9_]* or _+[A-Za-z][A-Za-z0-9_]*
};

// Returns a human-readable description of a TokenType for diagnostics.
absl::string_view TokenTypeName(TokenType type);

struct Token {
  TokenType type = TokenType::kEof;
  SourceLocation loc;
  absl::string_view lexeme;  // Raw slice in the underlying input buffer.
  int64_t int_value = 0;     // Populated when type == kInteger.
  double float_value = 0.0;  // Populated when type == kFloatValue.
  std::string string_value;  // Populated when type == kString or kError.
};

// Zero-copy tokenizer for FlatZinc (.fzn) input text.
// Specification:
// https://docs.minizinc.org/en/stable/fzn-spec.html#specification-of-flatzinc
// The caller must ensure the input buffer outlives the Lexer and any Token
// lexemes read from it.
class Lexer {
 public:
  explicit Lexer(absl::string_view input) : input_(input) {}

  // Peeks at the next token without consuming it.
  const Token& Peek();

  // Consumes and returns the next token. Once kEof is reached, subsequent calls
  // continue returning kEof.
  Token Next();

 private:
  // Byte offset in `input_` paired with 1-based line/column coordinates.
  struct Cursor {
    size_t offset = 0;
    SourceLocation loc;
  };

  Token ScanNextToken();
  void SkipWhitespaceAndComments();
  Token ScanIdentifierOrKeyword();
  Token ScanNumber();
  Token ScanRadixInteger(bool negative, int base);
  Token ScanString();

  absl::string_view CurrentLexeme() const {
    return input_.substr(token_start_.offset,
                         cursor_.offset - token_start_.offset);
  }
  Token MakeToken(TokenType type) const;
  Token MakeError(std::string message) const;

  bool AtEnd() const { return cursor_.offset >= input_.size(); }
  char PeekChar(size_t offset = 0) const;
  char AdvanceChar();
  template <typename Pred>
  void AdvanceWhile(Pred&& pred);

  absl::string_view input_;
  // Half-open span `[token_start_, cursor_)` of the token being scanned.
  Cursor cursor_;
  Cursor token_start_;
  std::optional<Token> peeked_;
};

}  // namespace operations_research::fz

#endif  // ORTOOLS_FLATZINC_LEXER_H_
