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

#include <cstdint>
#include <limits>
#include <vector>

#include "absl/strings/string_view.h"
#include "gtest/gtest.h"
#include "ortools/base/gmock.h"

namespace operations_research::fz {
namespace {

using ::testing::DoubleEq;
using ::testing::ElementsAre;
using ::testing::HasSubstr;

std::vector<TokenType> LexAllTypes(absl::string_view input) {
  Lexer lexer(input);
  std::vector<TokenType> types;
  while (true) {
    const Token tok = lexer.Next();
    types.push_back(tok.type);
    if (tok.type == TokenType::kEof || tok.type == TokenType::kError) {
      break;
    }
  }
  return types;
}

TEST(LexerTest, EmptyAndCommentsYieldEof) {
  Lexer empty("");
  EXPECT_EQ(empty.Peek().type, TokenType::kEof);
  EXPECT_EQ(empty.Next().type, TokenType::kEof);
  EXPECT_EQ(empty.Next().type, TokenType::kEof);

  Lexer comments("  \t\r\n% first comment\r\n  % trailing comment without nl");
  const Token eof = comments.Next();
  EXPECT_EQ(eof.type, TokenType::kEof);
  EXPECT_EQ(eof.loc, (SourceLocation{3, 32}));
}

TEST(LexerTest, PunctuationAndMultiCharOperators) {
  EXPECT_THAT(
      LexAllTypes("; : :: , .. ( ) [ ] { } = :::"),
      ElementsAre(TokenType::kSemicolon, TokenType::kColon,
                  TokenType::kColonColon, TokenType::kComma, TokenType::kDotDot,
                  TokenType::kLParen, TokenType::kRParen, TokenType::kLBracket,
                  TokenType::kRBracket, TokenType::kLBrace, TokenType::kRBrace,
                  TokenType::kEqual, TokenType::kColonColon, TokenType::kColon,
                  TokenType::kEof));
}

TEST(LexerTest, KeywordsAndBooleanLiterals) {
  EXPECT_THAT(
      LexAllTypes("array bool constraint float int maximize minimize of "
                  "predicate satisfy set solve var"),
      ElementsAre(TokenType::kArray, TokenType::kBool, TokenType::kConstraint,
                  TokenType::kFloat, TokenType::kInt, TokenType::kMaximize,
                  TokenType::kMinimize, TokenType::kOf, TokenType::kPredicate,
                  TokenType::kSatisfy, TokenType::kSet, TokenType::kSolve,
                  TokenType::kVar, TokenType::kEof));

  Lexer bools("true false");
  const Token t = bools.Next();
  EXPECT_EQ(t.type, TokenType::kInteger);
  EXPECT_EQ(t.lexeme, "true");
  EXPECT_EQ(t.int_value, 1);

  const Token f = bools.Next();
  EXPECT_EQ(f.type, TokenType::kInteger);
  EXPECT_EQ(f.lexeme, "false");
  EXPECT_EQ(f.int_value, 0);
  EXPECT_EQ(bools.Next().type, TokenType::kEof);
}

TEST(LexerTest, IdentifiersAndLeadingUnderscores) {
  Lexer lexer("x X_INTRODUCED_0_ _tmp __a1 array_var");
  Token tok = lexer.Next();
  EXPECT_EQ(tok.type, TokenType::kIdentifier);
  EXPECT_EQ(tok.lexeme, "x");

  tok = lexer.Next();
  EXPECT_EQ(tok.type, TokenType::kIdentifier);
  EXPECT_EQ(tok.lexeme, "X_INTRODUCED_0_");

  tok = lexer.Next();
  EXPECT_EQ(tok.type, TokenType::kIdentifier);
  EXPECT_EQ(tok.lexeme, "_tmp");

  tok = lexer.Next();
  EXPECT_EQ(tok.type, TokenType::kIdentifier);
  EXPECT_EQ(tok.lexeme, "__a1");

  tok = lexer.Next();
  EXPECT_EQ(tok.type, TokenType::kIdentifier);
  EXPECT_EQ(tok.lexeme, "array_var");

  EXPECT_EQ(lexer.Next().type, TokenType::kEof);
}

TEST(LexerTest, RejectsInvalidUnderscoreIdentifiers) {
  Lexer bare_underscore("_");
  Token err = bare_underscore.Next();
  EXPECT_EQ(err.type, TokenType::kError);
  EXPECT_THAT(err.string_value, HasSubstr("Invalid identifier"));

  Lexer underscore_digits("__123");
  err = underscore_digits.Next();
  EXPECT_EQ(err.type, TokenType::kError);
  EXPECT_THAT(err.string_value, HasSubstr("Invalid identifier"));
}

TEST(LexerTest, DecimalHexAndOctalIntegersIncludingExtremes) {
  Lexer lexer(
      "0 -42 +7 9223372036854775807 -9223372036854775808 "
      "0x0 0x1aF -0x10 0x7fffffffffffffff -0x8000000000000000 "
      "0o0 0o755 -0o10 0o777777777777777777777 -0o1000000000000000000000");

  auto next_int = [&lexer]() {
    Token tok = lexer.Next();
    EXPECT_EQ(tok.type, TokenType::kInteger) << tok.string_value;
    return tok.int_value;
  };

  EXPECT_EQ(next_int(), 0);
  EXPECT_EQ(next_int(), -42);
  EXPECT_EQ(next_int(), 7);
  EXPECT_EQ(next_int(), std::numeric_limits<int64_t>::max());
  EXPECT_EQ(next_int(), std::numeric_limits<int64_t>::min());

  EXPECT_EQ(next_int(), 0);
  EXPECT_EQ(next_int(), 0x1af);
  EXPECT_EQ(next_int(), -16);
  EXPECT_EQ(next_int(), std::numeric_limits<int64_t>::max());
  EXPECT_EQ(next_int(), std::numeric_limits<int64_t>::min());

  EXPECT_EQ(next_int(), 0);
  EXPECT_EQ(next_int(), 0755);
  EXPECT_EQ(next_int(), -8);
  EXPECT_EQ(next_int(), std::numeric_limits<int64_t>::max());
  EXPECT_EQ(next_int(), std::numeric_limits<int64_t>::min());

  EXPECT_EQ(lexer.Next().type, TokenType::kEof);
}

TEST(LexerTest, IntegerOverflowAndMalformedNumbersReturnError) {
  for (absl::string_view bad : {
           "9223372036854775808",
           "-9223372036854775809",
           "0x8000000000000000",
           "-0x8000000000000001",
           "0o1000000000000000000000",
           "-0o1000000000000000000001",
           "0x",
           "0x1g",
           "0o8",
           "123abc",
           "1.;",
           "1.5e;",
           "1e999",
       }) {
    Lexer lexer(bad);
    const Token tok = lexer.Next();
    EXPECT_EQ(tok.type, TokenType::kError) << "Input: " << bad;
  }
}

TEST(LexerTest, FloatLiteralsAndIntervalDisambiguation) {
  Lexer floats("0.0 -0.0 3.14159 -4.5e-6 2E+10 10e3");
  auto next_float = [&floats]() {
    Token tok = floats.Next();
    EXPECT_EQ(tok.type, TokenType::kFloatValue) << tok.string_value;
    return tok.float_value;
  };
  EXPECT_THAT(next_float(), DoubleEq(0.0));
  EXPECT_THAT(next_float(), DoubleEq(-0.0));
  EXPECT_THAT(next_float(), DoubleEq(3.14159));
  EXPECT_THAT(next_float(), DoubleEq(-4.5e-6));
  EXPECT_THAT(next_float(), DoubleEq(2e10));
  EXPECT_THAT(next_float(), DoubleEq(10000.0));
  EXPECT_EQ(floats.Next().type, TokenType::kEof);

  // Critical: '1..10' must lex as integer, '..', integer (not malformed '1.').
  Lexer int_range("1..10 -5..-1");
  Token tok = int_range.Next();
  EXPECT_EQ(tok.type, TokenType::kInteger);
  EXPECT_EQ(tok.int_value, 1);
  EXPECT_EQ(int_range.Next().type, TokenType::kDotDot);
  tok = int_range.Next();
  EXPECT_EQ(tok.type, TokenType::kInteger);
  EXPECT_EQ(tok.int_value, 10);

  tok = int_range.Next();
  EXPECT_EQ(tok.type, TokenType::kInteger);
  EXPECT_EQ(tok.int_value, -5);
  EXPECT_EQ(int_range.Next().type, TokenType::kDotDot);
  tok = int_range.Next();
  EXPECT_EQ(tok.type, TokenType::kInteger);
  EXPECT_EQ(tok.int_value, -1);
  EXPECT_EQ(int_range.Next().type, TokenType::kEof);

  // Float intervals such as '-0.0..-0.0'.
  Lexer float_range("-0.0..-0.0");
  EXPECT_EQ(float_range.Next().type, TokenType::kFloatValue);
  EXPECT_EQ(float_range.Next().type, TokenType::kDotDot);
  EXPECT_EQ(float_range.Next().type, TokenType::kFloatValue);
  EXPECT_EQ(float_range.Next().type, TokenType::kEof);
}

TEST(LexerTest, StringLiteralsUnquoteAndUnescape) {
  Lexer lexer(R"("" "hello" "quote: \" sq: \' slash: \\ tab:\t nl:\n cr:\r")");

  Token tok = lexer.Next();
  EXPECT_EQ(tok.type, TokenType::kString);
  EXPECT_EQ(tok.string_value, "");

  tok = lexer.Next();
  EXPECT_EQ(tok.type, TokenType::kString);
  EXPECT_EQ(tok.string_value, "hello");

  tok = lexer.Next();
  EXPECT_EQ(tok.type, TokenType::kString);
  EXPECT_EQ(tok.string_value, "quote: \" sq: ' slash: \\ tab:\t nl:\n cr:\r");

  EXPECT_EQ(lexer.Next().type, TokenType::kEof);
}

TEST(LexerTest, RejectsUnterminatedAndBadEscapeStrings) {
  for (absl::string_view bad :
       {absl::string_view("\"unterminated"),
        absl::string_view("\"line\nbreak\""),
        absl::string_view("\"cr\rbreak\""), absl::string_view("\"trailing\\"),
        absl::string_view(R"("\q")")}) {
    Lexer lexer(bad);
    EXPECT_EQ(lexer.Next().type, TokenType::kError) << "Input: " << bad;
  }
}

TEST(LexerTest, Utf8AllowedInStringsAndCommentsRejectedElsewhere) {
  Lexer valid("% comment with utf8: café\n\"string with utf8: 日本語\"");
  const Token str = valid.Next();
  EXPECT_EQ(str.type, TokenType::kString);
  EXPECT_EQ(str.string_value, "string with utf8: 日本語");
  EXPECT_EQ(valid.Next().type, TokenType::kEof);

  for (absl::string_view bad : {".", "@", "$", "\x01", "é"}) {
    Lexer lexer(bad);
    EXPECT_EQ(lexer.Next().type, TokenType::kError) << "Input: " << bad;
  }
}

TEST(LexerTest, TracksSourceLocationsAcrossLinesAndPeek) {
  Lexer lexer("var 1..10: x;\r\n% comment\nsolve satisfy;");

  EXPECT_EQ(lexer.Peek().loc, (SourceLocation{1, 1}));
  EXPECT_EQ(lexer.Next().type, TokenType::kVar);

  const Token lb = lexer.Next();
  EXPECT_EQ(lb.loc, (SourceLocation{1, 5}));
  EXPECT_EQ(lexer.Next().loc, (SourceLocation{1, 6}));   // ..
  EXPECT_EQ(lexer.Next().loc, (SourceLocation{1, 8}));   // 10
  EXPECT_EQ(lexer.Next().loc, (SourceLocation{1, 10}));  // :
  EXPECT_EQ(lexer.Next().loc, (SourceLocation{1, 12}));  // x
  EXPECT_EQ(lexer.Next().loc, (SourceLocation{1, 13}));  // ;

  const Token solve = lexer.Next();
  EXPECT_EQ(solve.type, TokenType::kSolve);
  EXPECT_EQ(solve.loc, (SourceLocation{3, 1}));

  const Token satisfy = lexer.Next();
  EXPECT_EQ(satisfy.type, TokenType::kSatisfy);
  EXPECT_EQ(satisfy.loc, (SourceLocation{3, 7}));
}

}  // namespace
}  // namespace operations_research::fz
