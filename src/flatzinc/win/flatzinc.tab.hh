/* A Bison parser, made by GNU Bison 2.5.  */

/* Bison interface for Yacc-like parsers in C
   
      Copyright (C) 1984, 1989-1990, 2000-2011 Free Software Foundation, Inc.
   
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


/* Tokens.  */
#ifndef YYTOKENTYPE
# define YYTOKENTYPE
   /* Put the tokens into the symbol table, so that GDB and other debuggers
      know about them.  */
   enum yytokentype {
     FZ_INT_LIT = 258,
     FZ_BOOL_LIT = 259,
     FZ_FLOAT_LIT = 260,
     FZ_ID = 261,
     FZ_U_ID = 262,
     FZ_STRING_LIT = 263,
     FZ_VAR = 264,
     FZ_PAR = 265,
     FZ_ANNOTATION = 266,
     FZ_ANY = 267,
     FZ_ARRAY = 268,
     FZ_BOOL = 269,
     FZ_CASE = 270,
     FZ_COLONCOLON = 271,
     FZ_CONSTRAINT = 272,
     FZ_DEFAULT = 273,
     FZ_DOTDOT = 274,
     FZ_ELSE = 275,
     FZ_ELSEIF = 276,
     FZ_ENDIF = 277,
     FZ_ENUM = 278,
     FZ_FLOAT = 279,
     FZ_FUNCTION = 280,
     FZ_IF = 281,
     FZ_INCLUDE = 282,
     FZ_INT = 283,
     FZ_LET = 284,
     FZ_MAXIMIZE = 285,
     FZ_MINIMIZE = 286,
     FZ_OF = 287,
     FZ_SATISFY = 288,
     FZ_OUTPUT = 289,
     FZ_PREDICATE = 290,
     FZ_RECORD = 291,
     FZ_SET = 292,
     FZ_SHOW = 293,
     FZ_SHOWCOND = 294,
     FZ_SOLVE = 295,
     FZ_STRING = 296,
     FZ_TEST = 297,
     FZ_THEN = 298,
     FZ_TUPLE = 299,
     FZ_TYPE = 300,
     FZ_VARIANT_RECORD = 301,
     FZ_WHERE = 302
   };
#endif



#if ! defined YYSTYPE && ! defined YYSTYPE_IS_DECLARED
typedef union YYSTYPE
{

/* Line 2068 of yacc.c  */
#line 79 "src/flatzinc/flatzinc.yy"

  int64 iValue;
  char* sValue;
  bool bValue;
  double dValue;
  std::vector<int64>* setValue;
  operations_research::AstSetLit* setLit;
  std::vector<double>* floatSetValue;
  std::vector<operations_research::AstSetLit>* setValueList;
  operations_research::Option<operations_research::AstSetLit*> oSet;
  operations_research::IntVarSpec* varIntSpec;
  operations_research::BoolVarSpec* varBoolSpec;
  operations_research::SetVarSpec* varSetSpec;
  operations_research::FloatVarSpec* varFloatSpec;
  operations_research::Option<operations_research::AstNode*> oArg;
  std::vector<operations_research::IntVarSpec*>* varIntSpecVec;
  std::vector<operations_research::BoolVarSpec*>* varBoolSpecVec;
  std::vector<operations_research::SetVarSpec*>* varSetSpecVec;
  std::vector<operations_research::FloatVarSpec*>* varFloatSpecVec;
  operations_research::Option<std::vector<operations_research::IntVarSpec*>*> oIntVarSpecVec;
  operations_research::Option<std::vector<operations_research::BoolVarSpec*>*> oBoolVarSpecVec;
  operations_research::Option<std::vector<operations_research::SetVarSpec*>*> oSetVarSpecVec;
  operations_research::Option<std::vector<operations_research::FloatVarSpec*>*> oFloatVarSpecVec;
  operations_research::AstNode* arg;
  operations_research::AstArray* argVec;



/* Line 2068 of yacc.c  */
#line 126 "src/flatzinc/win/flatzinc.tab.hh"
} YYSTYPE;
# define YYSTYPE_IS_TRIVIAL 1
# define yystype YYSTYPE /* obsolescent; will be withdrawn */
# define YYSTYPE_IS_DECLARED 1
#endif




