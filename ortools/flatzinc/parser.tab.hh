/* A Bison parser, made by GNU Bison 3.3.2.  */

/* Bison interface for Yacc-like parsers in C

   Copyright (C) 1984, 1989-1990, 2000-2015, 2018-2019 Free Software Foundation,
   Inc.

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.  */

/* As a special exception, you may create a larger work that contains
   part or all of the Bison parser skeleton and distribute that work
   under terms of your choice, so long as that work isn't itself a
   parser generator using the skeleton or a modified version thereof
   as a parser skeleton.  Alternatively, if you modify or redistribute
   the parser skeleton itself, you may (at your option) remove this
   special exception, which will cause the skeleton and the resulting
   Bison output files to be licensed under the GNU General Public
   License without this special exception.

   This special exception was added by the Free Software Foundation in
   version 2.2 of Bison.  */

/* Undocumented macros, especially those whose name start with YY_,
   are private implementation details.  Do not rely on them.  */

#ifndef YY_ORFZ_ORTOOLS_FLATZINC_PARSER_TAB_HH_INCLUDED
#define YY_ORFZ_ORTOOLS_FLATZINC_PARSER_TAB_HH_INCLUDED
/* Debug traces.  */
#ifndef ORFZ_DEBUG
#if defined YYDEBUG
#if YYDEBUG
#define ORFZ_DEBUG 1
#else
#define ORFZ_DEBUG 0
#endif
#else /* ! defined YYDEBUG */
#define ORFZ_DEBUG 1
#endif /* ! defined YYDEBUG */
#endif /* ! defined ORFZ_DEBUG */
#if ORFZ_DEBUG
extern int orfz_debug;
#endif
/* "%code requires" blocks.  */
#line 21 "./ortools/flatzinc/parser.yy" /* yacc.c:1921  */

#if !defined(OR_TOOLS_FLATZINC_FLATZINC_TAB_HH_)
#define OR_TOOLS_FLATZINC_FLATZINC_TAB_HH_
#include "absl/strings/match.h"
#include "absl/strings/str_format.h"
#include "ortools/flatzinc/parser_util.h"

// Tells flex to use the LexerInfo class to communicate with the bison parser.
typedef operations_research::fz::LexerInfo YYSTYPE;

// Defines the parameter to the orfz_lex() call from the orfz_parse() method.
#define YYLEX_PARAM scanner

#endif  // OR_TOOLS_FLATZINC_FLATZINC_TAB_HH_

#line 72 "./ortools/flatzinc/parser.tab.hh" /* yacc.c:1921  */

/* Token type.  */
#ifndef ORFZ_TOKENTYPE
#define ORFZ_TOKENTYPE
enum orfz_tokentype {
  ARRAY = 258,
  TOKEN_BOOL = 259,
  CONSTRAINT = 260,
  TOKEN_FLOAT = 261,
  TOKEN_INT = 262,
  MAXIMIZE = 263,
  MINIMIZE = 264,
  OF = 265,
  PREDICATE = 266,
  SATISFY = 267,
  SET = 268,
  SOLVE = 269,
  VAR = 270,
  DOTDOT = 271,
  COLONCOLON = 272,
  IVALUE = 273,
  SVALUE = 274,
  IDENTIFIER = 275,
  DVALUE = 276
};
#endif

/* Value type.  */

int orfz_parse(operations_research::fz::ParserContext* context,
               operations_research::fz::Model* model, bool* ok, void* scanner);

#endif /* !YY_ORFZ_ORTOOLS_FLATZINC_PARSER_TAB_HH_INCLUDED  */
