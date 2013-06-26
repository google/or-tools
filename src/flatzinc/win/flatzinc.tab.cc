/* A Bison parser, made by GNU Bison 2.3.  */

/* Skeleton implementation for Bison's Yacc-like parsers in C

   Copyright (C) 1984, 1989, 1990, 2000, 2001, 2002, 2003, 2004, 2005, 2006
   Free Software Foundation, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.  */

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

/* C LALR(1) parser skeleton written by Richard Stallman, by
   simplifying the original so-called "semantic" parser.  */

/* All symbols defined below should begin with yy or YY, to avoid
   infringing on user name space.  This should be done even for local
   variables, as they might otherwise be expanded by user macros.
   There are some unavoidable exceptions within include files to
   define necessary library symbols; they are noted "INFRINGES ON
   USER NAME SPACE" below.  */

/* Identify Bison output.  */
#define YYBISON 1

/* Bison version.  */
#define YYBISON_VERSION "2.3"

/* Skeleton name.  */
#define YYSKELETON_NAME "yacc.c"

/* Pure parsers.  */
#define YYPURE 1

/* Using locations.  */
#define YYLSP_NEEDED 0

/* Substitute the variable and function names.  */
#define yyparse orfz_parse
#define yylex   orfz_lex
#define yyerror orfz_error
#define yylval  orfz_lval
#define yychar  orfz_char
#define yydebug orfz_debug
#define yynerrs orfz_nerrs


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
/* Tokens.  */
#define FZ_INT_LIT 258
#define FZ_BOOL_LIT 259
#define FZ_FLOAT_LIT 260
#define FZ_ID 261
#define FZ_U_ID 262
#define FZ_STRING_LIT 263
#define FZ_VAR 264
#define FZ_PAR 265
#define FZ_ANNOTATION 266
#define FZ_ANY 267
#define FZ_ARRAY 268
#define FZ_BOOL 269
#define FZ_CASE 270
#define FZ_COLONCOLON 271
#define FZ_CONSTRAINT 272
#define FZ_DEFAULT 273
#define FZ_DOTDOT 274
#define FZ_ELSE 275
#define FZ_ELSEIF 276
#define FZ_ENDIF 277
#define FZ_ENUM 278
#define FZ_FLOAT 279
#define FZ_FUNCTION 280
#define FZ_IF 281
#define FZ_INCLUDE 282
#define FZ_INT 283
#define FZ_LET 284
#define FZ_MAXIMIZE 285
#define FZ_MINIMIZE 286
#define FZ_OF 287
#define FZ_SATISFY 288
#define FZ_OUTPUT 289
#define FZ_PREDICATE 290
#define FZ_RECORD 291
#define FZ_SET 292
#define FZ_SHOW 293
#define FZ_SHOWCOND 294
#define FZ_SOLVE 295
#define FZ_STRING 296
#define FZ_TEST 297
#define FZ_THEN 298
#define FZ_TUPLE 299
#define FZ_TYPE 300
#define FZ_VARIANT_RECORD 301
#define FZ_WHERE 302




/* Copy the first part of user declarations.  */
#line 39 "src/flatzinc/flatzinc.yy"

#define YYPARSE_PARAM parm
#define YYLEX_PARAM static_cast<ParserState*>(parm)->yyscanner
#include "flatzinc/flatzinc.h"
#include "flatzinc/parser.h"
#include "flatzinc/flatzinc.tab.hh"

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#if !defined(_MSC_VER)
#include <unistd.h>
#include <sys/mman.h>
#endif
#include <sys/types.h>
#include <sys/stat.h>

  extern int orfz_lex(YYSTYPE*, void* scanner);
  extern int orfz_get_lineno (void* scanner);
  extern int orfz_debug;

  using namespace operations_research;

  void orfz_error(void* parm, const char* str) {
    ParserState* const pp = static_cast<ParserState*>(parm);
    LOG(ERROR) << "Error: " << str
               << " in line no. " << orfz_get_lineno(pp->yyscanner);
    pp->hadError = true;
  }

  void orfz_assert(ParserState* const pp, bool cond, const char* str)
  {
    if (!cond) {
      LOG(ERROR) << "Error: " << str
                 << " in line no. " << orfz_get_lineno(pp->yyscanner);
      pp->hadError = true;
    }
  }
  

/* Enabling traces.  */
#ifndef YYDEBUG
# define YYDEBUG 1
#endif

/* Enabling verbose error messages.  */
#ifdef YYERROR_VERBOSE
# undef YYERROR_VERBOSE
# define YYERROR_VERBOSE 1
#else
# define YYERROR_VERBOSE 1
#endif

/* Enabling the token table.  */
#ifndef YYTOKEN_TABLE
# define YYTOKEN_TABLE 0
#endif

#if ! defined YYSTYPE && ! defined YYSTYPE_IS_DECLARED
typedef union YYSTYPE
#line 79 "src/flatzinc/flatzinc.yy"
{
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
}
/* Line 193 of yacc.c.  */
#line 265 "src/flatzinc/win/flatzinc.tab.cc"
	YYSTYPE;
# define yystype YYSTYPE /* obsolescent; will be withdrawn */
# define YYSTYPE_IS_DECLARED 1
# define YYSTYPE_IS_TRIVIAL 1
#endif



/* Copy the second part of user declarations.  */


/* Line 216 of yacc.c.  */
#line 278 "src/flatzinc/win/flatzinc.tab.cc"

#ifdef short
# undef short
#endif

#ifdef YYTYPE_UINT8
typedef YYTYPE_UINT8 yytype_uint8;
#else
typedef unsigned char yytype_uint8;
#endif

#ifdef YYTYPE_INT8
typedef YYTYPE_INT8 yytype_int8;
#elif (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
typedef signed char yytype_int8;
#else
typedef short int yytype_int8;
#endif

#ifdef YYTYPE_UINT16
typedef YYTYPE_UINT16 yytype_uint16;
#else
typedef unsigned short int yytype_uint16;
#endif

#ifdef YYTYPE_INT16
typedef YYTYPE_INT16 yytype_int16;
#else
typedef short int yytype_int16;
#endif

#ifndef YYSIZE_T
# ifdef __SIZE_TYPE__
#  define YYSIZE_T __SIZE_TYPE__
# elif defined size_t
#  define YYSIZE_T size_t
# elif ! defined YYSIZE_T && (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
#  include <stddef.h> /* INFRINGES ON USER NAME SPACE */
#  define YYSIZE_T size_t
# else
#  define YYSIZE_T unsigned int
# endif
#endif

#define YYSIZE_MAXIMUM ((YYSIZE_T) -1)

#ifndef YY_
# if defined YYENABLE_NLS && YYENABLE_NLS
#  if ENABLE_NLS
#   include <libintl.h> /* INFRINGES ON USER NAME SPACE */
#   define YY_(msgid) dgettext ("bison-runtime", msgid)
#  endif
# endif
# ifndef YY_
#  define YY_(msgid) msgid
# endif
#endif

/* Suppress unused-variable warnings by "using" E.  */
#if ! defined lint || defined __GNUC__
# define YYUSE(e) ((void) (e))
#else
# define YYUSE(e) /* empty */
#endif

/* Identity function, used to suppress warnings about constant conditions.  */
#ifndef lint
# define YYID(n) (n)
#else
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static int
YYID (int i)
#else
static int
YYID (i)
    int i;
#endif
{
  return i;
}
#endif

#if ! defined yyoverflow || YYERROR_VERBOSE

/* The parser invokes alloca or malloc; define the necessary symbols.  */

# ifdef YYSTACK_USE_ALLOCA
#  if YYSTACK_USE_ALLOCA
#   ifdef __GNUC__
#    define YYSTACK_ALLOC __builtin_alloca
#   elif defined __BUILTIN_VA_ARG_INCR
#    include <alloca.h> /* INFRINGES ON USER NAME SPACE */
#   elif defined _AIX
#    define YYSTACK_ALLOC __alloca
#   elif defined _MSC_VER
#    include <malloc.h> /* INFRINGES ON USER NAME SPACE */
#    define alloca _alloca
#   else
#    define YYSTACK_ALLOC alloca
#    if ! defined _ALLOCA_H && ! defined _STDLIB_H && (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
#     include <stdlib.h> /* INFRINGES ON USER NAME SPACE */
#     ifndef _STDLIB_H
#      define _STDLIB_H 1
#     endif
#    endif
#   endif
#  endif
# endif

# ifdef YYSTACK_ALLOC
   /* Pacify GCC's `empty if-body' warning.  */
#  define YYSTACK_FREE(Ptr) do { /* empty */; } while (YYID (0))
#  ifndef YYSTACK_ALLOC_MAXIMUM
    /* The OS might guarantee only one guard page at the bottom of the stack,
       and a page size can be as small as 4096 bytes.  So we cannot safely
       invoke alloca (N) if N exceeds 4096.  Use a slightly smaller number
       to allow for a few compiler-allocated temporary stack slots.  */
#   define YYSTACK_ALLOC_MAXIMUM 4032 /* reasonable circa 2006 */
#  endif
# else
#  define YYSTACK_ALLOC YYMALLOC
#  define YYSTACK_FREE YYFREE
#  ifndef YYSTACK_ALLOC_MAXIMUM
#   define YYSTACK_ALLOC_MAXIMUM YYSIZE_MAXIMUM
#  endif
#  if (defined __cplusplus && ! defined _STDLIB_H \
       && ! ((defined YYMALLOC || defined malloc) \
	     && (defined YYFREE || defined free)))
#   include <stdlib.h> /* INFRINGES ON USER NAME SPACE */
#   ifndef _STDLIB_H
#    define _STDLIB_H 1
#   endif
#  endif
#  ifndef YYMALLOC
#   define YYMALLOC malloc
#   if ! defined malloc && ! defined _STDLIB_H && (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
void *malloc (YYSIZE_T); /* INFRINGES ON USER NAME SPACE */
#   endif
#  endif
#  ifndef YYFREE
#   define YYFREE free
#   if ! defined free && ! defined _STDLIB_H && (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
void free (void *); /* INFRINGES ON USER NAME SPACE */
#   endif
#  endif
# endif
#endif /* ! defined yyoverflow || YYERROR_VERBOSE */


#if (! defined yyoverflow \
     && (! defined __cplusplus \
	 || (defined YYSTYPE_IS_TRIVIAL && YYSTYPE_IS_TRIVIAL)))

/* A type that is properly aligned for any stack member.  */
union yyalloc
{
  yytype_int16 yyss;
  YYSTYPE yyvs;
  };

/* The size of the maximum gap between one aligned stack and the next.  */
# define YYSTACK_GAP_MAXIMUM (sizeof (union yyalloc) - 1)

/* The size of an array large to enough to hold all stacks, each with
   N elements.  */
# define YYSTACK_BYTES(N) \
     ((N) * (sizeof (yytype_int16) + sizeof (YYSTYPE)) \
      + YYSTACK_GAP_MAXIMUM)

/* Copy COUNT objects from FROM to TO.  The source and destination do
   not overlap.  */
# ifndef YYCOPY
#  if defined __GNUC__ && 1 < __GNUC__
#   define YYCOPY(To, From, Count) \
      __builtin_memcpy (To, From, (Count) * sizeof (*(From)))
#  else
#   define YYCOPY(To, From, Count)		\
      do					\
	{					\
	  YYSIZE_T yyi;				\
	  for (yyi = 0; yyi < (Count); yyi++)	\
	    (To)[yyi] = (From)[yyi];		\
	}					\
      while (YYID (0))
#  endif
# endif

/* Relocate STACK from its old location to the new one.  The
   local variables YYSIZE and YYSTACKSIZE give the old and new number of
   elements in the stack, and YYPTR gives the new location of the
   stack.  Advance YYPTR to a properly aligned location for the next
   stack.  */
# define YYSTACK_RELOCATE(Stack)					\
    do									\
      {									\
	YYSIZE_T yynewbytes;						\
	YYCOPY (&yyptr->Stack, Stack, yysize);				\
	Stack = &yyptr->Stack;						\
	yynewbytes = yystacksize * sizeof (*Stack) + YYSTACK_GAP_MAXIMUM; \
	yyptr += yynewbytes / sizeof (*yyptr);				\
      }									\
    while (YYID (0))

#endif

/* YYFINAL -- State number of the termination state.  */
#define YYFINAL  7
/* YYLAST -- Last index in YYTABLE.  */
#define YYLAST   337

/* YYNTOKENS -- Number of terminals.  */
#define YYNTOKENS  58
/* YYNNTS -- Number of nonterminals.  */
#define YYNNTS  67
/* YYNRULES -- Number of rules.  */
#define YYNRULES  156
/* YYNRULES -- Number of states.  */
#define YYNSTATES  336

/* YYTRANSLATE(YYLEX) -- Bison symbol number corresponding to YYLEX.  */
#define YYUNDEFTOK  2
#define YYMAXUTOK   302

#define YYTRANSLATE(YYX)						\
  ((unsigned int) (YYX) <= YYMAXUTOK ? yytranslate[YYX] : YYUNDEFTOK)

/* YYTRANSLATE[YYLEX] -- Bison symbol number corresponding to YYLEX.  */
static const yytype_uint8 yytranslate[] =
{
       0,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
      49,    50,     2,     2,    51,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,    52,    48,
       2,    55,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,    53,     2,    54,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,    56,     2,    57,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     1,     2,     3,     4,
       5,     6,     7,     8,     9,    10,    11,    12,    13,    14,
      15,    16,    17,    18,    19,    20,    21,    22,    23,    24,
      25,    26,    27,    28,    29,    30,    31,    32,    33,    34,
      35,    36,    37,    38,    39,    40,    41,    42,    43,    44,
      45,    46,    47
};

#if YYDEBUG
/* YYPRHS[YYN] -- Index of the first RHS symbol of rule number YYN in
   YYRHS.  */
static const yytype_uint16 yyprhs[] =
{
       0,     0,     3,     9,    10,    12,    15,    19,    20,    22,
      25,    29,    30,    32,    35,    39,    45,    46,    49,    51,
      55,    59,    66,    74,    77,    79,    81,    85,    87,    89,
      91,    95,    97,   101,   103,   105,   112,   119,   126,   135,
     142,   149,   158,   172,   186,   200,   216,   232,   248,   264,
     282,   284,   286,   291,   292,   295,   297,   301,   302,   304,
     308,   310,   312,   317,   318,   321,   323,   327,   331,   333,
     335,   340,   341,   344,   346,   350,   354,   356,   358,   363,
     364,   367,   369,   373,   377,   378,   381,   382,   385,   386,
     389,   390,   393,   400,   404,   409,   411,   415,   419,   421,
     426,   428,   433,   437,   441,   442,   445,   447,   451,   452,
     455,   457,   461,   462,   465,   467,   471,   472,   475,   477,
     481,   483,   487,   489,   493,   494,   497,   499,   501,   503,
     505,   507,   512,   513,   516,   518,   522,   524,   529,   531,
     533,   534,   536,   539,   543,   548,   550,   552,   556,   558,
     562,   564,   566,   568,   570,   572,   577
};

/* YYRHS -- A `-1'-separated list of the rules' RHS.  */
static const yytype_int8 yyrhs[] =
{
      59,     0,    -1,    60,    62,    64,    98,    48,    -1,    -1,
      61,    -1,    66,    48,    -1,    61,    66,    48,    -1,    -1,
      63,    -1,    75,    48,    -1,    63,    75,    48,    -1,    -1,
      65,    -1,    97,    48,    -1,    65,    97,    48,    -1,    35,
       6,    49,    67,    50,    -1,    -1,    68,    79,    -1,    69,
      -1,    68,    51,    69,    -1,    70,    52,     6,    -1,    13,
      53,    72,    54,    32,    71,    -1,    13,    53,    72,    54,
      32,     9,    71,    -1,     9,    71,    -1,    71,    -1,    99,
      -1,    37,    32,    99,    -1,    14,    -1,    24,    -1,    73,
      -1,    72,    51,    73,    -1,    28,    -1,     3,    19,     3,
      -1,     6,    -1,     7,    -1,     9,    99,    52,    74,   119,
     113,    -1,     9,   100,    52,    74,   119,   113,    -1,     9,
     101,    52,    74,   119,   113,    -1,     9,    37,    32,    99,
      52,    74,   119,   113,    -1,    28,    52,    74,   119,    55,
     114,    -1,    14,    52,    74,   119,    55,   114,    -1,    37,
      32,    28,    52,    74,   119,    55,   114,    -1,    13,    53,
       3,    19,     3,    54,    32,     9,    99,    52,    74,   119,
      93,    -1,    13,    53,     3,    19,     3,    54,    32,     9,
     100,    52,    74,   119,    94,    -1,    13,    53,     3,    19,
       3,    54,    32,     9,   101,    52,    74,   119,    95,    -1,
      13,    53,     3,    19,     3,    54,    32,     9,    37,    32,
      99,    52,    74,   119,    96,    -1,    13,    53,     3,    19,
       3,    54,    32,    28,    52,    74,   119,    55,    53,   103,
      54,    -1,    13,    53,     3,    19,     3,    54,    32,    14,
      52,    74,   119,    55,    53,   105,    54,    -1,    13,    53,
       3,    19,     3,    54,    32,    24,    52,    74,   119,    55,
      53,   107,    54,    -1,    13,    53,     3,    19,     3,    54,
      32,    37,    32,    28,    52,    74,   119,    55,    53,   109,
      54,    -1,     3,    -1,    74,    -1,    74,    53,     3,    54,
      -1,    -1,    78,    79,    -1,    76,    -1,    78,    51,    76,
      -1,    -1,    51,    -1,    53,    77,    54,    -1,     5,    -1,
      74,    -1,    74,    53,     3,    54,    -1,    -1,    83,    79,
      -1,    81,    -1,    83,    51,    81,    -1,    53,    82,    54,
      -1,     4,    -1,    74,    -1,    74,    53,     3,    54,    -1,
      -1,    87,    79,    -1,    85,    -1,    87,    51,    85,    -1,
      53,    86,    54,    -1,   102,    -1,    74,    -1,    74,    53,
       3,    54,    -1,    -1,    91,    79,    -1,    89,    -1,    91,
      51,    89,    -1,    53,    90,    54,    -1,    -1,    55,    80,
      -1,    -1,    55,    88,    -1,    -1,    55,    84,    -1,    -1,
      55,    92,    -1,    17,     6,    49,   111,    50,   119,    -1,
      40,   119,    33,    -1,    40,   119,   118,   117,    -1,    28,
      -1,    56,   103,    57,    -1,     3,    19,     3,    -1,    14,
      -1,    56,   106,    79,    57,    -1,    24,    -1,    56,   108,
      79,    57,    -1,    56,   103,    57,    -1,     3,    19,     3,
      -1,    -1,   104,    79,    -1,     3,    -1,   104,    51,     3,
      -1,    -1,   106,    79,    -1,     4,    -1,   106,    51,     4,
      -1,    -1,   108,    79,    -1,     5,    -1,   108,    51,     5,
      -1,    -1,   110,    79,    -1,   102,    -1,   110,    51,   102,
      -1,   112,    -1,   111,    51,   112,    -1,   114,    -1,    53,
     115,    54,    -1,    -1,    55,   114,    -1,     4,    -1,     3,
      -1,     5,    -1,   102,    -1,    74,    -1,    74,    53,   114,
      54,    -1,    -1,   116,    79,    -1,   114,    -1,   116,    51,
     114,    -1,    74,    -1,    74,    53,     3,    54,    -1,    31,
      -1,    30,    -1,    -1,   120,    -1,    16,   121,    -1,   120,
      16,   121,    -1,     6,    49,   122,    50,    -1,   123,    -1,
     121,    -1,   122,    51,   121,    -1,   124,    -1,    53,   122,
      54,    -1,     4,    -1,     3,    -1,     5,    -1,   102,    -1,
      74,    -1,    74,    53,   124,    54,    -1,     8,    -1
};

/* YYRLINE[YYN] -- source line where rule number YYN was defined.  */
static const yytype_uint16 yyrline[] =
{
       0,   195,   195,   197,   199,   202,   203,   207,   208,   212,
     213,   215,   217,   220,   221,   228,   230,   232,   235,   236,
     239,   242,   243,   244,   245,   248,   249,   250,   251,   254,
     255,   258,   259,   265,   265,   268,   295,   324,   329,   359,
     366,   373,   382,   449,   499,   506,   561,   574,   587,   594,
     608,   612,   626,   649,   650,   654,   656,   659,   659,   661,
     665,   667,   681,   704,   705,   709,   711,   715,   719,   721,
     735,   758,   759,   763,   765,   768,   771,   773,   787,   810,
     811,   815,   817,   820,   825,   826,   831,   832,   837,   838,
     843,   844,   848,   861,   875,   898,   900,   902,   908,   910,
     923,   924,   931,   933,   940,   941,   945,   947,   952,   953,
     957,   959,   964,   965,   969,   971,   976,   977,   981,   983,
     991,   993,   997,   999,  1004,  1005,  1009,  1011,  1013,  1015,
    1017,  1066,  1080,  1081,  1085,  1087,  1095,  1107,  1127,  1128,
    1136,  1137,  1141,  1143,  1147,  1151,  1155,  1157,  1161,  1163,
    1167,  1169,  1171,  1173,  1175,  1223,  1234
};
#endif

#if YYDEBUG || YYERROR_VERBOSE || YYTOKEN_TABLE
/* YYTNAME[SYMBOL-NUM] -- String name of the symbol SYMBOL-NUM.
   First, the terminals, then, starting at YYNTOKENS, nonterminals.  */
static const char *const yytname[] =
{
  "$end", "error", "$undefined", "FZ_INT_LIT", "FZ_BOOL_LIT",
  "FZ_FLOAT_LIT", "FZ_ID", "FZ_U_ID", "FZ_STRING_LIT", "FZ_VAR", "FZ_PAR",
  "FZ_ANNOTATION", "FZ_ANY", "FZ_ARRAY", "FZ_BOOL", "FZ_CASE",
  "FZ_COLONCOLON", "FZ_CONSTRAINT", "FZ_DEFAULT", "FZ_DOTDOT", "FZ_ELSE",
  "FZ_ELSEIF", "FZ_ENDIF", "FZ_ENUM", "FZ_FLOAT", "FZ_FUNCTION", "FZ_IF",
  "FZ_INCLUDE", "FZ_INT", "FZ_LET", "FZ_MAXIMIZE", "FZ_MINIMIZE", "FZ_OF",
  "FZ_SATISFY", "FZ_OUTPUT", "FZ_PREDICATE", "FZ_RECORD", "FZ_SET",
  "FZ_SHOW", "FZ_SHOWCOND", "FZ_SOLVE", "FZ_STRING", "FZ_TEST", "FZ_THEN",
  "FZ_TUPLE", "FZ_TYPE", "FZ_VARIANT_RECORD", "FZ_WHERE", "';'", "'('",
  "')'", "','", "':'", "'['", "']'", "'='", "'{'", "'}'", "$accept",
  "model", "preddecl_items", "preddecl_items_head", "vardecl_items",
  "vardecl_items_head", "constraint_items", "constraint_items_head",
  "preddecl_item", "pred_arg_list", "pred_arg_list_head", "pred_arg",
  "pred_arg_type", "pred_arg_simple_type", "pred_array_init",
  "pred_array_init_arg", "var_par_id", "vardecl_item", "int_init",
  "int_init_list", "int_init_list_head", "list_tail",
  "int_var_array_literal", "float_init", "float_init_list",
  "float_init_list_head", "float_var_array_literal", "bool_init",
  "bool_init_list", "bool_init_list_head", "bool_var_array_literal",
  "set_init", "set_init_list", "set_init_list_head",
  "set_var_array_literal", "vardecl_int_var_array_init",
  "vardecl_bool_var_array_init", "vardecl_float_var_array_init",
  "vardecl_set_var_array_init", "constraint_item", "solve_item",
  "int_ti_expr_tail", "bool_ti_expr_tail", "float_ti_expr_tail",
  "set_literal", "int_list", "int_list_head", "bool_list",
  "bool_list_head", "float_list", "float_list_head", "set_literal_list",
  "set_literal_list_head", "flat_expr_list", "flat_expr",
  "non_array_expr_opt", "non_array_expr", "non_array_expr_list",
  "non_array_expr_list_head", "solve_expr", "minmax", "annotations",
  "annotations_head", "annotation", "annotation_list", "annotation_expr",
  "ann_non_array_expr", 0
};
#endif

# ifdef YYPRINT
/* YYTOKNUM[YYLEX-NUM] -- Internal token number corresponding to
   token YYLEX-NUM.  */
static const yytype_uint16 yytoknum[] =
{
       0,   256,   257,   258,   259,   260,   261,   262,   263,   264,
     265,   266,   267,   268,   269,   270,   271,   272,   273,   274,
     275,   276,   277,   278,   279,   280,   281,   282,   283,   284,
     285,   286,   287,   288,   289,   290,   291,   292,   293,   294,
     295,   296,   297,   298,   299,   300,   301,   302,    59,    40,
      41,    44,    58,    91,    93,    61,   123,   125
};
# endif

/* YYR1[YYN] -- Symbol number of symbol that rule YYN derives.  */
static const yytype_uint8 yyr1[] =
{
       0,    58,    59,    60,    60,    61,    61,    62,    62,    63,
      63,    64,    64,    65,    65,    66,    67,    67,    68,    68,
      69,    70,    70,    70,    70,    71,    71,    71,    71,    72,
      72,    73,    73,    74,    74,    75,    75,    75,    75,    75,
      75,    75,    75,    75,    75,    75,    75,    75,    75,    75,
      76,    76,    76,    77,    77,    78,    78,    79,    79,    80,
      81,    81,    81,    82,    82,    83,    83,    84,    85,    85,
      85,    86,    86,    87,    87,    88,    89,    89,    89,    90,
      90,    91,    91,    92,    93,    93,    94,    94,    95,    95,
      96,    96,    97,    98,    98,    99,    99,    99,   100,   100,
     101,   101,   102,   102,   103,   103,   104,   104,   105,   105,
     106,   106,   107,   107,   108,   108,   109,   109,   110,   110,
     111,   111,   112,   112,   113,   113,   114,   114,   114,   114,
     114,   114,   115,   115,   116,   116,   117,   117,   118,   118,
     119,   119,   120,   120,   121,   121,   122,   122,   123,   123,
     124,   124,   124,   124,   124,   124,   124
};

/* YYR2[YYN] -- Number of symbols composing right hand side of rule YYN.  */
static const yytype_uint8 yyr2[] =
{
       0,     2,     5,     0,     1,     2,     3,     0,     1,     2,
       3,     0,     1,     2,     3,     5,     0,     2,     1,     3,
       3,     6,     7,     2,     1,     1,     3,     1,     1,     1,
       3,     1,     3,     1,     1,     6,     6,     6,     8,     6,
       6,     8,    13,    13,    13,    15,    15,    15,    15,    17,
       1,     1,     4,     0,     2,     1,     3,     0,     1,     3,
       1,     1,     4,     0,     2,     1,     3,     3,     1,     1,
       4,     0,     2,     1,     3,     3,     1,     1,     4,     0,
       2,     1,     3,     3,     0,     2,     0,     2,     0,     2,
       0,     2,     6,     3,     4,     1,     3,     3,     1,     4,
       1,     4,     3,     3,     0,     2,     1,     3,     0,     2,
       1,     3,     0,     2,     1,     3,     0,     2,     1,     3,
       1,     3,     1,     3,     0,     2,     1,     1,     1,     1,
       1,     4,     0,     2,     1,     3,     1,     4,     1,     1,
       0,     1,     2,     3,     4,     1,     1,     3,     1,     3,
       1,     1,     1,     1,     1,     4,     1
};

/* YYDEFACT[STATE-NAME] -- Default rule to reduce with in state
   STATE-NUM when YYTABLE doesn't specify something else to do.  Zero
   means the default is an error.  */
static const yytype_uint8 yydefact[] =
{
       3,     0,     0,     7,     4,     0,     0,     1,     0,     0,
       0,     0,     0,    11,     8,     0,     0,     5,    16,     0,
      98,   100,    95,     0,   104,     0,     0,     0,     0,     0,
       0,     0,     0,     0,    12,     0,     0,     9,     6,     0,
       0,    27,    28,     0,   104,     0,    57,    18,     0,    24,
      25,     0,     0,   106,   110,   114,     0,    57,    57,    57,
       0,     0,     0,     0,    33,    34,   140,   140,     0,     0,
     140,     0,     0,    13,    10,    23,     0,     0,    15,    58,
      17,     0,    97,     0,    96,    58,   105,    58,     0,    58,
       0,   140,   140,   140,     0,     0,     0,   141,     0,     0,
       0,     0,     2,    14,     0,    31,     0,    29,    26,    19,
      20,     0,   107,   111,    99,   115,   101,   124,   124,   124,
       0,   151,   150,   152,    33,   156,     0,   104,   154,   153,
     142,   145,   148,     0,     0,     0,   140,   127,   126,   128,
     132,   130,   129,     0,   120,   122,   139,   138,    93,     0,
       0,     0,     0,   140,     0,    35,    36,    37,     0,     0,
       0,   146,     0,     0,     0,    40,   143,    39,     0,   134,
       0,    57,     0,   140,     0,   136,    94,    32,    30,     0,
     124,   125,     0,   103,     0,     0,   149,   102,     0,     0,
     123,    58,   133,     0,    92,   121,     0,     0,    21,    38,
       0,     0,     0,     0,     0,   144,   147,   155,    41,   135,
     131,     0,    22,     0,     0,     0,     0,     0,     0,     0,
       0,   137,     0,     0,     0,     0,   140,   140,   140,     0,
       0,   140,   140,   140,     0,     0,     0,     0,     0,    84,
      86,    88,     0,     0,     0,   140,   140,     0,    42,     0,
      43,     0,    44,   108,   112,   104,     0,    90,    53,    85,
      71,    87,    63,    89,     0,    57,     0,    57,     0,     0,
       0,    45,    50,    51,    55,     0,    57,    68,    69,    73,
       0,    57,    60,    61,    65,     0,    57,    47,   109,    48,
     113,    46,   116,    79,    91,     0,    59,    58,    54,     0,
      75,    58,    72,     0,    67,    58,    64,     0,   118,     0,
      57,    77,    81,     0,    57,    76,     0,    56,     0,    74,
       0,    66,    49,    58,   117,     0,    83,    58,    80,    52,
      70,    62,   119,     0,    82,    78
};

/* YYDEFGOTO[NTERM-NUM].  */
static const yytype_int16 yydefgoto[] =
{
      -1,     2,     3,     4,    13,    14,    33,    34,     5,    45,
      46,    47,    48,    49,   106,   107,   141,    15,   274,   275,
     276,    80,   259,   284,   285,   286,   263,   279,   280,   281,
     261,   312,   313,   314,   294,   248,   250,   252,   271,    35,
      71,    50,    26,    27,   142,    56,    57,   264,    58,   266,
      59,   309,   310,   143,   144,   155,   145,   170,   171,   176,
     149,    96,    97,   161,   162,   131,   132
};

/* YYPACT[STATE-NUM] -- Index in YYTABLE of the portion describing
   STATE-NUM.  */
#define YYPACT_NINF -120
static const yytype_int16 yypact[] =
{
       0,    34,    46,   101,     0,     1,    12,  -120,    97,    -3,
      29,    33,    71,    96,   101,    68,    74,  -120,    50,   100,
    -120,  -120,  -120,    94,    64,    76,    78,    80,   136,    23,
      23,   112,   151,   119,    96,   113,   114,  -120,  -120,   144,
     110,  -120,  -120,   132,   163,   117,   118,  -120,   126,  -120,
    -120,   167,    24,  -120,  -120,  -120,   122,   120,   129,   131,
      23,    23,    23,   164,  -120,  -120,   168,   168,   133,   137,
     168,   139,   149,  -120,  -120,  -120,    15,    24,  -120,    50,
    -120,   186,  -120,   146,  -120,   198,  -120,   201,   150,   209,
     158,   168,   168,   168,   203,     9,   162,   202,   165,    23,
      51,    53,  -120,  -120,   200,  -120,   -15,  -120,  -120,  -120,
    -120,    23,  -120,  -120,  -120,  -120,  -120,   169,   169,   169,
     174,   204,  -120,  -120,   187,  -120,     9,   163,   182,  -120,
    -120,  -120,  -120,   170,     9,   170,   168,   204,  -120,  -120,
     170,   184,  -120,    44,  -120,  -120,  -120,  -120,  -120,    23,
     236,    15,   208,   168,   170,  -120,  -120,  -120,   210,   238,
       9,  -120,   -10,   189,    85,  -120,  -120,  -120,   190,  -120,
     193,   192,   170,   168,    51,   195,  -120,  -120,  -120,   109,
     169,  -120,    10,  -120,    58,     9,  -120,  -120,   196,   170,
    -120,   170,  -120,   197,  -120,  -120,   246,   144,  -120,  -120,
     188,   205,   206,   207,   220,  -120,  -120,  -120,  -120,  -120,
    -120,   199,  -120,   222,   211,   213,   214,    23,    23,    23,
     227,  -120,    24,    23,    23,    23,   168,   168,   168,   215,
     217,   168,   168,   168,   216,   218,   219,    23,    23,   223,
     224,   225,   228,   229,   233,   168,   168,   234,  -120,   239,
    -120,   240,  -120,   257,   265,   163,   241,   242,    19,  -120,
     148,  -120,   138,  -120,   221,   129,   237,   131,   245,   247,
     248,  -120,  -120,   251,  -120,   252,   226,  -120,   254,  -120,
     255,   243,  -120,   258,  -120,   256,   244,  -120,  -120,  -120,
    -120,  -120,    17,    95,  -120,   259,  -120,    19,  -120,   302,
    -120,   148,  -120,   305,  -120,   138,  -120,   204,  -120,   260,
     261,   262,  -120,   263,   267,  -120,   266,  -120,   268,  -120,
     269,  -120,  -120,    17,  -120,   310,  -120,    95,  -120,  -120,
    -120,  -120,  -120,   270,  -120,  -120
};

/* YYPGOTO[NTERM-NUM].  */
static const yytype_int16 yypgoto[] =
{
    -120,  -120,  -120,  -120,  -120,  -120,  -120,  -120,   312,  -120,
    -120,   249,  -120,   -37,  -120,   175,   -29,   307,    22,  -120,
    -120,   -54,  -120,    20,  -120,  -120,  -120,    26,  -120,  -120,
    -120,     2,  -120,  -120,  -120,  -120,  -120,  -120,  -120,   296,
    -120,    -1,   134,   135,   -89,  -119,  -120,  -120,    79,  -120,
      77,  -120,  -120,  -120,   159,  -108,  -112,  -120,  -120,  -120,
    -120,    57,  -120,   -86,   176,  -120,   173
};

/* YYTABLE[YYPACT[STATE-NUM]].  What to do in state STATE-NUM.  If
   positive, shift that token.  If negative, reduce the rule which
   number is the opposite.  If zero, do what YYDEFACT says.
   If YYTABLE_NINF, syntax error.  */
#define YYTABLE_NINF -1
static const yytype_uint16 yytable[] =
{
      66,    67,    75,    86,    88,    90,   129,    25,   163,   130,
     156,   157,   121,   122,   123,   124,    65,   125,   104,   200,
     307,   165,   272,   167,   201,    64,    65,    19,   169,    64,
      65,    91,    92,    93,   202,     1,   151,   129,   203,   152,
       6,   185,   181,   105,   186,   129,     7,   204,   166,    17,
      28,    83,    22,    19,   137,   138,   139,    64,    65,    39,
     193,    18,   126,    40,    41,   127,   128,    53,    54,    55,
     136,   129,   199,   127,    42,   129,   108,   208,    22,   209,
      44,    29,   153,   146,   147,    30,   148,    43,   121,   122,
     123,    64,    65,   125,   173,   174,   129,   128,   307,   206,
      19,    64,    65,    31,   140,   128,    44,   127,   205,   185,
       8,    20,    19,    32,     9,    10,    37,   192,   197,    51,
     175,    21,    38,    41,    98,    22,    52,   101,    60,    11,
      61,   128,    62,    42,    23,   128,   268,    22,    12,    63,
      68,   127,   198,   282,    64,    65,    43,    19,   117,   118,
     119,   127,   277,    24,    64,    65,   128,    69,    41,    70,
     212,    73,    74,    76,    77,    44,    53,    78,    42,    79,
      82,    85,    22,   137,   138,   139,    64,    65,    81,    84,
      87,    43,    89,    94,    95,    99,   100,   102,   226,   227,
     228,    19,   110,   168,   231,   232,   233,   103,   111,   214,
      44,   112,    20,   308,   315,   113,   120,   114,   245,   246,
     180,   288,    21,   290,   115,   116,    22,   133,   134,   150,
     135,   230,   298,   159,   154,   213,   127,   302,   158,   273,
     194,   278,   306,   283,   332,   164,   160,   172,   315,   177,
     179,   183,   182,   191,    24,   189,   187,   190,   196,   211,
     207,   210,   220,   221,   222,   229,   324,   217,   218,   219,
     328,    54,   316,   223,   311,   224,   225,   237,   273,   238,
      55,   242,   278,   243,   244,   287,   283,   297,   247,   249,
     251,   253,   254,   234,   235,   236,   255,   258,   239,   240,
     241,   289,   260,   262,   301,   305,   269,   270,   311,   291,
     292,   293,   256,   257,   295,   318,   296,   299,   320,   300,
     304,   303,   323,   333,   322,   325,    16,   326,   327,   317,
     329,    36,   330,   331,   335,   321,   178,   319,   109,   334,
      72,   267,   265,   195,   215,   216,   184,   188
};

static const yytype_uint16 yycheck[] =
{
      29,    30,    39,    57,    58,    59,    95,     8,   127,    95,
     118,   119,     3,     4,     5,     6,     7,     8,     3,     9,
       3,   133,     3,   135,    14,     6,     7,     3,   140,     6,
       7,    60,    61,    62,    24,    35,    51,   126,    28,    54,
       6,    51,   154,    28,    54,   134,     0,    37,   134,    48,
      53,    52,    28,     3,     3,     4,     5,     6,     7,     9,
     172,    49,    53,    13,    14,    56,    95,     3,     4,     5,
      99,   160,   180,    56,    24,   164,    77,   189,    28,   191,
      56,    52,   111,    30,    31,    52,    33,    37,     3,     4,
       5,     6,     7,     8,    50,    51,   185,   126,     3,   185,
       3,     6,     7,    32,    53,   134,    56,    56,    50,    51,
       9,    14,     3,    17,    13,    14,    48,   171,     9,    19,
     149,    24,    48,    14,    67,    28,    32,    70,    52,    28,
      52,   160,    52,    24,    37,   164,   255,    28,    37,     3,
      28,    56,   179,     5,     6,     7,    37,     3,    91,    92,
      93,    56,     4,    56,     6,     7,   185,     6,    14,    40,
     197,    48,    48,    53,    32,    56,     3,    50,    24,    51,
       3,    51,    28,     3,     4,     5,     6,     7,    52,    57,
      51,    37,    51,    19,    16,    52,    49,    48,   217,   218,
     219,     3,     6,   136,   223,   224,   225,    48,    52,   200,
      56,     3,    14,   292,   293,     4,     3,    57,   237,   238,
     153,   265,    24,   267,     5,    57,    28,    55,    16,    19,
      55,   222,   276,    19,    55,    37,    56,   281,    54,   258,
     173,   260,   286,   262,   323,    53,    49,    53,   327,     3,
      32,     3,    32,    51,    56,    55,    57,    54,    53,     3,
      54,    54,    32,    54,    32,    28,   310,    52,    52,    52,
     314,     4,     3,    52,   293,    52,    52,    52,   297,    52,
       5,    55,   301,    55,    55,    54,   305,    51,    55,    55,
      55,    53,    53,   226,   227,   228,    53,    53,   231,   232,
     233,    54,    53,    53,    51,    51,    55,    55,   327,    54,
      53,    53,   245,   246,    53,     3,    54,    53,     3,    54,
      54,    53,    51,     3,    54,    53,     4,    54,    51,   297,
      54,    14,    54,    54,    54,   305,   151,   301,    79,   327,
      34,   254,   253,   174,   200,   200,   160,   164
};

/* YYSTOS[STATE-NUM] -- The (internal number of the) accessing
   symbol of state STATE-NUM.  */
static const yytype_uint8 yystos[] =
{
       0,    35,    59,    60,    61,    66,     6,     0,     9,    13,
      14,    28,    37,    62,    63,    75,    66,    48,    49,     3,
      14,    24,    28,    37,    56,    99,   100,   101,    53,    52,
      52,    32,    17,    64,    65,    97,    75,    48,    48,     9,
      13,    14,    24,    37,    56,    67,    68,    69,    70,    71,
      99,    19,    32,     3,     4,     5,   103,   104,   106,   108,
      52,    52,    52,     3,     6,     7,    74,    74,    28,     6,
      40,    98,    97,    48,    48,    71,    53,    32,    50,    51,
      79,    52,     3,    99,    57,    51,    79,    51,    79,    51,
      79,    74,    74,    74,    19,    16,   119,   120,   119,    52,
      49,   119,    48,    48,     3,    28,    72,    73,    99,    69,
       6,    52,     3,     4,    57,     5,    57,   119,   119,   119,
       3,     3,     4,     5,     6,     8,    53,    56,    74,   102,
     121,   123,   124,    55,    16,    55,    74,     3,     4,     5,
      53,    74,   102,   111,   112,   114,    30,    31,    33,   118,
      19,    51,    54,    74,    55,   113,   113,   113,    54,    19,
      49,   121,   122,   103,    53,   114,   121,   114,   119,   114,
     115,   116,    53,    50,    51,    74,   117,     3,    73,    32,
     119,   114,    32,     3,   122,    51,    54,    57,   124,    55,
      54,    51,    79,   114,   119,   112,    53,     9,    71,   113,
       9,    14,    24,    28,    37,    50,   121,    54,   114,   114,
      54,     3,    71,    37,    99,   100,   101,    52,    52,    52,
      32,    54,    32,    52,    52,    52,    74,    74,    74,    28,
      99,    74,    74,    74,   119,   119,   119,    52,    52,   119,
     119,   119,    55,    55,    55,    74,    74,    55,    93,    55,
      94,    55,    95,    53,    53,    53,   119,   119,    53,    80,
      53,    88,    53,    84,   105,   106,   107,   108,   103,    55,
      55,    96,     3,    74,    76,    77,    78,     4,    74,    85,
      86,    87,     5,    74,    81,    82,    83,    54,    79,    54,
      79,    54,    53,    53,    92,    53,    54,    51,    79,    53,
      54,    51,    79,    53,    54,    51,    79,     3,   102,   109,
     110,    74,    89,    90,    91,   102,     3,    76,     3,    85,
       3,    81,    54,    51,    79,    53,    54,    51,    79,    54,
      54,    54,   102,     3,    89,    54
};

#define yyerrok		(yyerrstatus = 0)
#define yyclearin	(yychar = YYEMPTY)
#define YYEMPTY		(-2)
#define YYEOF		0

#define YYACCEPT	goto yyacceptlab
#define YYABORT		goto yyabortlab
#define YYERROR		goto yyerrorlab


/* Like YYERROR except do call yyerror.  This remains here temporarily
   to ease the transition to the new meaning of YYERROR, for GCC.
   Once GCC version 2 has supplanted version 1, this can go.  */

#define YYFAIL		goto yyerrlab

#define YYRECOVERING()  (!!yyerrstatus)

#define YYBACKUP(Token, Value)					\
do								\
  if (yychar == YYEMPTY && yylen == 1)				\
    {								\
      yychar = (Token);						\
      yylval = (Value);						\
      yytoken = YYTRANSLATE (yychar);				\
      YYPOPSTACK (1);						\
      goto yybackup;						\
    }								\
  else								\
    {								\
      yyerror (parm, YY_("syntax error: cannot back up")); \
      YYERROR;							\
    }								\
while (YYID (0))


#define YYTERROR	1
#define YYERRCODE	256


/* YYLLOC_DEFAULT -- Set CURRENT to span from RHS[1] to RHS[N].
   If N is 0, then set CURRENT to the empty location which ends
   the previous symbol: RHS[0] (always defined).  */

#define YYRHSLOC(Rhs, K) ((Rhs)[K])
#ifndef YYLLOC_DEFAULT
# define YYLLOC_DEFAULT(Current, Rhs, N)				\
    do									\
      if (YYID (N))                                                    \
	{								\
	  (Current).first_line   = YYRHSLOC (Rhs, 1).first_line;	\
	  (Current).first_column = YYRHSLOC (Rhs, 1).first_column;	\
	  (Current).last_line    = YYRHSLOC (Rhs, N).last_line;		\
	  (Current).last_column  = YYRHSLOC (Rhs, N).last_column;	\
	}								\
      else								\
	{								\
	  (Current).first_line   = (Current).last_line   =		\
	    YYRHSLOC (Rhs, 0).last_line;				\
	  (Current).first_column = (Current).last_column =		\
	    YYRHSLOC (Rhs, 0).last_column;				\
	}								\
    while (YYID (0))
#endif


/* YY_LOCATION_PRINT -- Print the location on the stream.
   This macro was not mandated originally: define only if we know
   we won't break user code: when these are the locations we know.  */

#ifndef YY_LOCATION_PRINT
# if defined YYLTYPE_IS_TRIVIAL && YYLTYPE_IS_TRIVIAL
#  define YY_LOCATION_PRINT(File, Loc)			\
     fprintf (File, "%d.%d-%d.%d",			\
	      (Loc).first_line, (Loc).first_column,	\
	      (Loc).last_line,  (Loc).last_column)
# else
#  define YY_LOCATION_PRINT(File, Loc) ((void) 0)
# endif
#endif


/* YYLEX -- calling `yylex' with the right arguments.  */

#ifdef YYLEX_PARAM
# define YYLEX yylex (&yylval, YYLEX_PARAM)
#else
# define YYLEX yylex (&yylval)
#endif

/* Enable debugging if requested.  */
#if YYDEBUG

# ifndef YYFPRINTF
#  include <stdio.h> /* INFRINGES ON USER NAME SPACE */
#  define YYFPRINTF fprintf
# endif

# define YYDPRINTF(Args)			\
do {						\
  if (yydebug)					\
    YYFPRINTF Args;				\
} while (YYID (0))

# define YY_SYMBOL_PRINT(Title, Type, Value, Location)			  \
do {									  \
  if (yydebug)								  \
    {									  \
      YYFPRINTF (stderr, "%s ", Title);					  \
      yy_symbol_print (stderr,						  \
		  Type, Value, parm); \
      YYFPRINTF (stderr, "\n");						  \
    }									  \
} while (YYID (0))


/*--------------------------------.
| Print this symbol on YYOUTPUT.  |
`--------------------------------*/

/*ARGSUSED*/
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static void
yy_symbol_value_print (FILE *yyoutput, int yytype, YYSTYPE const * const yyvaluep, void* parm)
#else
static void
yy_symbol_value_print (yyoutput, yytype, yyvaluep, parm)
    FILE *yyoutput;
    int yytype;
    YYSTYPE const * const yyvaluep;
    void* parm;
#endif
{
  if (!yyvaluep)
    return;
  YYUSE (parm);
# ifdef YYPRINT
  if (yytype < YYNTOKENS)
    YYPRINT (yyoutput, yytoknum[yytype], *yyvaluep);
# else
  YYUSE (yyoutput);
# endif
  switch (yytype)
    {
      default:
	break;
    }
}


/*--------------------------------.
| Print this symbol on YYOUTPUT.  |
`--------------------------------*/

#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static void
yy_symbol_print (FILE *yyoutput, int yytype, YYSTYPE const * const yyvaluep, void* parm)
#else
static void
yy_symbol_print (yyoutput, yytype, yyvaluep, parm)
    FILE *yyoutput;
    int yytype;
    YYSTYPE const * const yyvaluep;
    void* parm;
#endif
{
  if (yytype < YYNTOKENS)
    YYFPRINTF (yyoutput, "token %s (", yytname[yytype]);
  else
    YYFPRINTF (yyoutput, "nterm %s (", yytname[yytype]);

  yy_symbol_value_print (yyoutput, yytype, yyvaluep, parm);
  YYFPRINTF (yyoutput, ")");
}

/*------------------------------------------------------------------.
| yy_stack_print -- Print the state stack from its BOTTOM up to its |
| TOP (included).                                                   |
`------------------------------------------------------------------*/

#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static void
yy_stack_print (yytype_int16 *bottom, yytype_int16 *top)
#else
static void
yy_stack_print (bottom, top)
    yytype_int16 *bottom;
    yytype_int16 *top;
#endif
{
  YYFPRINTF (stderr, "Stack now");
  for (; bottom <= top; ++bottom)
    YYFPRINTF (stderr, " %d", *bottom);
  YYFPRINTF (stderr, "\n");
}

# define YY_STACK_PRINT(Bottom, Top)				\
do {								\
  if (yydebug)							\
    yy_stack_print ((Bottom), (Top));				\
} while (YYID (0))


/*------------------------------------------------.
| Report that the YYRULE is going to be reduced.  |
`------------------------------------------------*/

#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static void
yy_reduce_print (YYSTYPE *yyvsp, int yyrule, void* parm)
#else
static void
yy_reduce_print (yyvsp, yyrule, parm)
    YYSTYPE *yyvsp;
    int yyrule;
    void* parm;
#endif
{
  int yynrhs = yyr2[yyrule];
  int yyi;
  unsigned long int yylno = yyrline[yyrule];
  YYFPRINTF (stderr, "Reducing stack by rule %d (line %lu):\n",
	     yyrule - 1, yylno);
  /* The symbols being reduced.  */
  for (yyi = 0; yyi < yynrhs; yyi++)
    {
      fprintf (stderr, "   $%d = ", yyi + 1);
      yy_symbol_print (stderr, yyrhs[yyprhs[yyrule] + yyi],
		       &(yyvsp[(yyi + 1) - (yynrhs)])
		       		       , parm);
      fprintf (stderr, "\n");
    }
}

# define YY_REDUCE_PRINT(Rule)		\
do {					\
  if (yydebug)				\
    yy_reduce_print (yyvsp, Rule, parm); \
} while (YYID (0))

/* Nonzero means print parse trace.  It is left uninitialized so that
   multiple parsers can coexist.  */
int yydebug;
#else /* !YYDEBUG */
# define YYDPRINTF(Args)
# define YY_SYMBOL_PRINT(Title, Type, Value, Location)
# define YY_STACK_PRINT(Bottom, Top)
# define YY_REDUCE_PRINT(Rule)
#endif /* !YYDEBUG */


/* YYINITDEPTH -- initial size of the parser's stacks.  */
#ifndef	YYINITDEPTH
# define YYINITDEPTH 200
#endif

/* YYMAXDEPTH -- maximum size the stacks can grow to (effective only
   if the built-in stack extension method is used).

   Do not make this value too large; the results are undefined if
   YYSTACK_ALLOC_MAXIMUM < YYSTACK_BYTES (YYMAXDEPTH)
   evaluated with infinite-precision integer arithmetic.  */

#ifndef YYMAXDEPTH
# define YYMAXDEPTH 10000
#endif



#if YYERROR_VERBOSE

# ifndef yystrlen
#  if defined __GLIBC__ && defined _STRING_H
#   define yystrlen strlen
#  else
/* Return the length of YYSTR.  */
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static YYSIZE_T
yystrlen (const char *yystr)
#else
static YYSIZE_T
yystrlen (yystr)
    const char *yystr;
#endif
{
  YYSIZE_T yylen;
  for (yylen = 0; yystr[yylen]; yylen++)
    continue;
  return yylen;
}
#  endif
# endif

# ifndef yystpcpy
#  if defined __GLIBC__ && defined _STRING_H && defined _GNU_SOURCE
#   define yystpcpy stpcpy
#  else
/* Copy YYSRC to YYDEST, returning the address of the terminating '\0' in
   YYDEST.  */
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static char *
yystpcpy (char *yydest, const char *yysrc)
#else
static char *
yystpcpy (yydest, yysrc)
    char *yydest;
    const char *yysrc;
#endif
{
  char *yyd = yydest;
  const char *yys = yysrc;

  while ((*yyd++ = *yys++) != '\0')
    continue;

  return yyd - 1;
}
#  endif
# endif

# ifndef yytnamerr
/* Copy to YYRES the contents of YYSTR after stripping away unnecessary
   quotes and backslashes, so that it's suitable for yyerror.  The
   heuristic is that double-quoting is unnecessary unless the string
   contains an apostrophe, a comma, or backslash (other than
   backslash-backslash).  YYSTR is taken from yytname.  If YYRES is
   null, do not copy; instead, return the length of what the result
   would have been.  */
static YYSIZE_T
yytnamerr (char *yyres, const char *yystr)
{
  if (*yystr == '"')
    {
      YYSIZE_T yyn = 0;
      char const *yyp = yystr;

      for (;;)
	switch (*++yyp)
	  {
	  case '\'':
	  case ',':
	    goto do_not_strip_quotes;

	  case '\\':
	    if (*++yyp != '\\')
	      goto do_not_strip_quotes;
	    /* Fall through.  */
	  default:
	    if (yyres)
	      yyres[yyn] = *yyp;
	    yyn++;
	    break;

	  case '"':
	    if (yyres)
	      yyres[yyn] = '\0';
	    return yyn;
	  }
    do_not_strip_quotes: ;
    }

  if (! yyres)
    return yystrlen (yystr);

  return yystpcpy (yyres, yystr) - yyres;
}
# endif

/* Copy into YYRESULT an error message about the unexpected token
   YYCHAR while in state YYSTATE.  Return the number of bytes copied,
   including the terminating null byte.  If YYRESULT is null, do not
   copy anything; just return the number of bytes that would be
   copied.  As a special case, return 0 if an ordinary "syntax error"
   message will do.  Return YYSIZE_MAXIMUM if overflow occurs during
   size calculation.  */
static YYSIZE_T
yysyntax_error (char *yyresult, int yystate, int yychar)
{
  int yyn = yypact[yystate];

  if (! (YYPACT_NINF < yyn && yyn <= YYLAST))
    return 0;
  else
    {
      int yytype = YYTRANSLATE (yychar);
      YYSIZE_T yysize0 = yytnamerr (0, yytname[yytype]);
      YYSIZE_T yysize = yysize0;
      YYSIZE_T yysize1;
      int yysize_overflow = 0;
      enum { YYERROR_VERBOSE_ARGS_MAXIMUM = 5 };
      char const *yyarg[YYERROR_VERBOSE_ARGS_MAXIMUM];
      int yyx;

# if 0
      /* This is so xgettext sees the translatable formats that are
	 constructed on the fly.  */
      YY_("syntax error, unexpected %s");
      YY_("syntax error, unexpected %s, expecting %s");
      YY_("syntax error, unexpected %s, expecting %s or %s");
      YY_("syntax error, unexpected %s, expecting %s or %s or %s");
      YY_("syntax error, unexpected %s, expecting %s or %s or %s or %s");
# endif
      char *yyfmt;
      char const *yyf;
      static char const yyunexpected[] = "syntax error, unexpected %s";
      static char const yyexpecting[] = ", expecting %s";
      static char const yyor[] = " or %s";
      char yyformat[sizeof yyunexpected
		    + sizeof yyexpecting - 1
		    + ((YYERROR_VERBOSE_ARGS_MAXIMUM - 2)
		       * (sizeof yyor - 1))];
      char const *yyprefix = yyexpecting;

      /* Start YYX at -YYN if negative to avoid negative indexes in
	 YYCHECK.  */
      int yyxbegin = yyn < 0 ? -yyn : 0;

      /* Stay within bounds of both yycheck and yytname.  */
      int yychecklim = YYLAST - yyn + 1;
      int yyxend = yychecklim < YYNTOKENS ? yychecklim : YYNTOKENS;
      int yycount = 1;

      yyarg[0] = yytname[yytype];
      yyfmt = yystpcpy (yyformat, yyunexpected);

      for (yyx = yyxbegin; yyx < yyxend; ++yyx)
	if (yycheck[yyx + yyn] == yyx && yyx != YYTERROR)
	  {
	    if (yycount == YYERROR_VERBOSE_ARGS_MAXIMUM)
	      {
		yycount = 1;
		yysize = yysize0;
		yyformat[sizeof yyunexpected - 1] = '\0';
		break;
	      }
	    yyarg[yycount++] = yytname[yyx];
	    yysize1 = yysize + yytnamerr (0, yytname[yyx]);
	    yysize_overflow |= (yysize1 < yysize);
	    yysize = yysize1;
	    yyfmt = yystpcpy (yyfmt, yyprefix);
	    yyprefix = yyor;
	  }

      yyf = YY_(yyformat);
      yysize1 = yysize + yystrlen (yyf);
      yysize_overflow |= (yysize1 < yysize);
      yysize = yysize1;

      if (yysize_overflow)
	return YYSIZE_MAXIMUM;

      if (yyresult)
	{
	  /* Avoid sprintf, as that infringes on the user's name space.
	     Don't have undefined behavior even if the translation
	     produced a string with the wrong number of "%s"s.  */
	  char *yyp = yyresult;
	  int yyi = 0;
	  while ((*yyp = *yyf) != '\0')
	    {
	      if (*yyp == '%' && yyf[1] == 's' && yyi < yycount)
		{
		  yyp += yytnamerr (yyp, yyarg[yyi++]);
		  yyf += 2;
		}
	      else
		{
		  yyp++;
		  yyf++;
		}
	    }
	}
      return yysize;
    }
}
#endif /* YYERROR_VERBOSE */


/*-----------------------------------------------.
| Release the memory associated to this symbol.  |
`-----------------------------------------------*/

/*ARGSUSED*/
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
static void
yydestruct (const char *yymsg, int yytype, YYSTYPE *yyvaluep, void* parm)
#else
static void
yydestruct (yymsg, yytype, yyvaluep, parm)
    const char *yymsg;
    int yytype;
    YYSTYPE *yyvaluep;
    void* parm;
#endif
{
  YYUSE (yyvaluep);
  YYUSE (parm);

  if (!yymsg)
    yymsg = "Deleting";
  YY_SYMBOL_PRINT (yymsg, yytype, yyvaluep, yylocationp);

  switch (yytype)
    {

      default:
	break;
    }
}


/* Prevent warnings from -Wmissing-prototypes.  */

#ifdef YYPARSE_PARAM
#if defined __STDC__ || defined __cplusplus
int yyparse (void *YYPARSE_PARAM);
#else
int yyparse ();
#endif
#else /* ! YYPARSE_PARAM */
#if defined __STDC__ || defined __cplusplus
int yyparse (void* parm);
#else
int yyparse ();
#endif
#endif /* ! YYPARSE_PARAM */






/*----------.
| yyparse.  |
`----------*/

#ifdef YYPARSE_PARAM
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
int
yyparse (void *YYPARSE_PARAM)
#else
int
yyparse (YYPARSE_PARAM)
    void *YYPARSE_PARAM;
#endif
#else /* ! YYPARSE_PARAM */
#if (defined __STDC__ || defined __C99__FUNC__ \
     || defined __cplusplus || defined _MSC_VER)
int
yyparse (void* parm)
#else
int
yyparse (parm)
    void* parm;
#endif
#endif
{
  /* The look-ahead symbol.  */
int yychar;

/* The semantic value of the look-ahead symbol.  */
YYSTYPE yylval;

/* Number of syntax errors so far.  */
int yynerrs;

  int yystate;
  int yyn;
  int yyresult;
  /* Number of tokens to shift before error messages enabled.  */
  int yyerrstatus;
  /* Look-ahead token as an internal (translated) token number.  */
  int yytoken = 0;
#if YYERROR_VERBOSE
  /* Buffer for error messages, and its allocated size.  */
  char yymsgbuf[128];
  char *yymsg = yymsgbuf;
  YYSIZE_T yymsg_alloc = sizeof yymsgbuf;
#endif

  /* Three stacks and their tools:
     `yyss': related to states,
     `yyvs': related to semantic values,
     `yyls': related to locations.

     Refer to the stacks thru separate pointers, to allow yyoverflow
     to reallocate them elsewhere.  */

  /* The state stack.  */
  yytype_int16 yyssa[YYINITDEPTH];
  yytype_int16 *yyss = yyssa;
  yytype_int16 *yyssp;

  /* The semantic value stack.  */
  YYSTYPE yyvsa[YYINITDEPTH];
  YYSTYPE *yyvs = yyvsa;
  YYSTYPE *yyvsp;



#define YYPOPSTACK(N)   (yyvsp -= (N), yyssp -= (N))

  YYSIZE_T yystacksize = YYINITDEPTH;

  /* The variables used to return semantic value and location from the
     action routines.  */
  YYSTYPE yyval;


  /* The number of symbols on the RHS of the reduced rule.
     Keep to zero when no symbol should be popped.  */
  int yylen = 0;

  YYDPRINTF ((stderr, "Starting parse\n"));

  yystate = 0;
  yyerrstatus = 0;
  yynerrs = 0;
  yychar = YYEMPTY;		/* Cause a token to be read.  */

  /* Initialize stack pointers.
     Waste one element of value and location stack
     so that they stay on the same level as the state stack.
     The wasted elements are never initialized.  */

  yyssp = yyss;
  yyvsp = yyvs;

  goto yysetstate;

/*------------------------------------------------------------.
| yynewstate -- Push a new state, which is found in yystate.  |
`------------------------------------------------------------*/
 yynewstate:
  /* In all cases, when you get here, the value and location stacks
     have just been pushed.  So pushing a state here evens the stacks.  */
  yyssp++;

 yysetstate:
  *yyssp = yystate;

  if (yyss + yystacksize - 1 <= yyssp)
    {
      /* Get the current used size of the three stacks, in elements.  */
      YYSIZE_T yysize = yyssp - yyss + 1;

#ifdef yyoverflow
      {
	/* Give user a chance to reallocate the stack.  Use copies of
	   these so that the &'s don't force the real ones into
	   memory.  */
	YYSTYPE *yyvs1 = yyvs;
	yytype_int16 *yyss1 = yyss;


	/* Each stack pointer address is followed by the size of the
	   data in use in that stack, in bytes.  This used to be a
	   conditional around just the two extra args, but that might
	   be undefined if yyoverflow is a macro.  */
	yyoverflow (YY_("memory exhausted"),
		    &yyss1, yysize * sizeof (*yyssp),
		    &yyvs1, yysize * sizeof (*yyvsp),

		    &yystacksize);

	yyss = yyss1;
	yyvs = yyvs1;
      }
#else /* no yyoverflow */
# ifndef YYSTACK_RELOCATE
      goto yyexhaustedlab;
# else
      /* Extend the stack our own way.  */
      if (YYMAXDEPTH <= yystacksize)
	goto yyexhaustedlab;
      yystacksize *= 2;
      if (YYMAXDEPTH < yystacksize)
	yystacksize = YYMAXDEPTH;

      {
	yytype_int16 *yyss1 = yyss;
	union yyalloc *yyptr =
	  (union yyalloc *) YYSTACK_ALLOC (YYSTACK_BYTES (yystacksize));
	if (! yyptr)
	  goto yyexhaustedlab;
	YYSTACK_RELOCATE (yyss);
	YYSTACK_RELOCATE (yyvs);

#  undef YYSTACK_RELOCATE
	if (yyss1 != yyssa)
	  YYSTACK_FREE (yyss1);
      }
# endif
#endif /* no yyoverflow */

      yyssp = yyss + yysize - 1;
      yyvsp = yyvs + yysize - 1;


      YYDPRINTF ((stderr, "Stack size increased to %lu\n",
		  (unsigned long int) yystacksize));

      if (yyss + yystacksize - 1 <= yyssp)
	YYABORT;
    }

  YYDPRINTF ((stderr, "Entering state %d\n", yystate));

  goto yybackup;

/*-----------.
| yybackup.  |
`-----------*/
yybackup:

  /* Do appropriate processing given the current state.  Read a
     look-ahead token if we need one and don't already have one.  */

  /* First try to decide what to do without reference to look-ahead token.  */
  yyn = yypact[yystate];
  if (yyn == YYPACT_NINF)
    goto yydefault;

  /* Not known => get a look-ahead token if don't already have one.  */

  /* YYCHAR is either YYEMPTY or YYEOF or a valid look-ahead symbol.  */
  if (yychar == YYEMPTY)
    {
      YYDPRINTF ((stderr, "Reading a token: "));
      yychar = YYLEX;
    }

  if (yychar <= YYEOF)
    {
      yychar = yytoken = YYEOF;
      YYDPRINTF ((stderr, "Now at end of input.\n"));
    }
  else
    {
      yytoken = YYTRANSLATE (yychar);
      YY_SYMBOL_PRINT ("Next token is", yytoken, &yylval, &yylloc);
    }

  /* If the proper action on seeing token YYTOKEN is to reduce or to
     detect an error, take that action.  */
  yyn += yytoken;
  if (yyn < 0 || YYLAST < yyn || yycheck[yyn] != yytoken)
    goto yydefault;
  yyn = yytable[yyn];
  if (yyn <= 0)
    {
      if (yyn == 0 || yyn == YYTABLE_NINF)
	goto yyerrlab;
      yyn = -yyn;
      goto yyreduce;
    }

  if (yyn == YYFINAL)
    YYACCEPT;

  /* Count tokens shifted since error; after three, turn off error
     status.  */
  if (yyerrstatus)
    yyerrstatus--;

  /* Shift the look-ahead token.  */
  YY_SYMBOL_PRINT ("Shifting", yytoken, &yylval, &yylloc);

  /* Discard the shifted token unless it is eof.  */
  if (yychar != YYEOF)
    yychar = YYEMPTY;

  yystate = yyn;
  *++yyvsp = yylval;

  goto yynewstate;


/*-----------------------------------------------------------.
| yydefault -- do the default action for the current state.  |
`-----------------------------------------------------------*/
yydefault:
  yyn = yydefact[yystate];
  if (yyn == 0)
    goto yyerrlab;
  goto yyreduce;


/*-----------------------------.
| yyreduce -- Do a reduction.  |
`-----------------------------*/
yyreduce:
  /* yyn is the number of a rule to reduce with.  */
  yylen = yyr2[yyn];

  /* If YYLEN is nonzero, implement the default value of the action:
     `$$ = $1'.

     Otherwise, the following line sets YYVAL to garbage.
     This behavior is undocumented and Bison
     users should not rely upon it.  Assigning to YYVAL
     unconditionally makes the parser a bit smaller, and it avoids a
     GCC warning that YYVAL may be used uninitialized.  */
  yyval = yyvsp[1-yylen];


  YY_REDUCE_PRINT (yyn);
  switch (yyn)
    {
        case 7:
#line 207 "src/flatzinc/flatzinc.yy"
    { static_cast<ParserState*>(parm)->InitModel(); ;}
    break;

  case 8:
#line 209 "src/flatzinc/flatzinc.yy"
    { static_cast<ParserState*>(parm)->InitModel(); ;}
    break;

  case 35:
#line 269 "src/flatzinc/flatzinc.yy"
    {
  ParserState* const pp = static_cast<ParserState*>(parm);
  bool print = (yyvsp[(5) - (6)].argVec)->hasAtom("output_var");
  bool introduced = (yyvsp[(5) - (6)].argVec)->hasAtom("var_is_introduced");
  pp->int_var_map_.put((yyvsp[(4) - (6)].sValue), pp->int_variables_.size());
  if (print) {
    pp->output(string((yyvsp[(4) - (6)].sValue)), new AstIntVar(pp->int_variables_.size()));
  }
  if ((yyvsp[(6) - (6)].oArg).defined()) {
    AstNode* arg = (yyvsp[(6) - (6)].oArg).value();
    if (arg->isInt()) {
      pp->int_variables_.push_back(
          new IntVarSpec((yyvsp[(4) - (6)].sValue), arg->getInt(), introduced));
    } else if (arg->isIntVar()) {
      pp->int_variables_.push_back(
          new IntVarSpec((yyvsp[(4) - (6)].sValue), Alias(arg->getIntVar()), introduced));
      pp->AddIntVarDomainConstraint(pp->int_variables_.size() - 1, (yyvsp[(2) - (6)].oSet).value());
    } else {
      orfz_assert(pp, false, "Invalid var int initializer.");
    }
    delete arg;
  } else {
    pp->int_variables_.push_back(new IntVarSpec((yyvsp[(4) - (6)].sValue), (yyvsp[(2) - (6)].oSet), introduced, true));
  }
  delete (yyvsp[(5) - (6)].argVec); free((yyvsp[(4) - (6)].sValue));
;}
    break;

  case 36:
#line 296 "src/flatzinc/flatzinc.yy"
    {
  ParserState* const pp = static_cast<ParserState*>(parm);
  bool print = (yyvsp[(5) - (6)].argVec)->hasAtom("output_var");
  bool introduced = (yyvsp[(5) - (6)].argVec)->hasAtom("var_is_introduced");
  pp->bool_var_map_.put((yyvsp[(4) - (6)].sValue), pp->bool_variables_.size());
  if (print) {
    pp->output(string((yyvsp[(4) - (6)].sValue)), new AstBoolVar(pp->bool_variables_.size()));
  }
  if ((yyvsp[(6) - (6)].oArg).defined()) {
    AstNode* arg = (yyvsp[(6) - (6)].oArg).value();
    if (arg->isBool()) {
      pp->bool_variables_.push_back(
          new BoolVarSpec((yyvsp[(4) - (6)].sValue), arg->getBool(),introduced));
    } else if (arg->isBoolVar()) {
      pp->bool_variables_.push_back(
          new BoolVarSpec((yyvsp[(4) - (6)].sValue), Alias(arg->getBoolVar()), introduced));
    } else {
      orfz_assert(pp, false, "Invalid var bool initializer.");
    }
    if (!pp->hadError) {
      pp->AddBoolVarDomainConstraint(pp->bool_variables_.size()-1, (yyvsp[(2) - (6)].oSet).value());
    }
    delete arg;
  } else {
    pp->bool_variables_.push_back(new BoolVarSpec((yyvsp[(4) - (6)].sValue), (yyvsp[(2) - (6)].oSet), introduced, true));
  }
  delete (yyvsp[(5) - (6)].argVec); free((yyvsp[(4) - (6)].sValue));
;}
    break;

  case 37:
#line 325 "src/flatzinc/flatzinc.yy"
    { ParserState* const pp = static_cast<ParserState*>(parm);
  orfz_assert(pp, false, "Floats not supported.");
  delete (yyvsp[(5) - (6)].argVec); free((yyvsp[(4) - (6)].sValue));
;}
    break;

  case 38:
#line 330 "src/flatzinc/flatzinc.yy"
    {
  ParserState* const pp = static_cast<ParserState*>(parm);
  bool print = (yyvsp[(7) - (8)].argVec)->hasAtom("output_var");
  bool introduced = (yyvsp[(7) - (8)].argVec)->hasAtom("var_is_introduced");
  pp->set_var_map_.put((yyvsp[(6) - (8)].sValue), pp->set_variables_.size());
  if (print) {
    pp->output(string((yyvsp[(6) - (8)].sValue)), new AstSetVar(pp->set_variables_.size()));
  }
  if ((yyvsp[(8) - (8)].oArg).defined()) {
    AstNode* arg = (yyvsp[(8) - (8)].oArg).value();
    if (arg->isSet()) {
      pp->set_variables_.push_back(
          new SetVarSpec((yyvsp[(6) - (8)].sValue), arg->getSet(),introduced));
    } else if (arg->isSetVar()) {
      pp->set_variables_.push_back(
          new SetVarSpec((yyvsp[(6) - (8)].sValue), Alias(arg->getSetVar()),introduced));
      delete arg;
    } else {
      orfz_assert(pp, false, "Invalid var set initializer.");
      delete arg;
    }
    if (!pp->hadError) {
      pp->AddSetVarDomainConstraint(pp->set_variables_.size() - 1, (yyvsp[(4) - (8)].oSet).value());
    }
  } else {
    pp->set_variables_.push_back(new SetVarSpec((yyvsp[(6) - (8)].sValue), (yyvsp[(4) - (8)].oSet),introduced, true));
  }
  delete (yyvsp[(7) - (8)].argVec); free((yyvsp[(6) - (8)].sValue));
;}
    break;

  case 39:
#line 360 "src/flatzinc/flatzinc.yy"
    {
  ParserState* const pp = static_cast<ParserState*>(parm);
  orfz_assert(pp, (yyvsp[(6) - (6)].arg)->isInt(), "Invalid int initializer.");
  pp->int_map_.put((yyvsp[(3) - (6)].sValue), (yyvsp[(6) - (6)].arg)->getInt());
  delete (yyvsp[(4) - (6)].argVec); free((yyvsp[(3) - (6)].sValue));
;}
    break;

  case 40:
#line 367 "src/flatzinc/flatzinc.yy"
    {
  ParserState* const pp = static_cast<ParserState*>(parm);
  orfz_assert(pp, (yyvsp[(6) - (6)].arg)->isBool(), "Invalid bool initializer.");
  pp->bool_map_.put((yyvsp[(3) - (6)].sValue), (yyvsp[(6) - (6)].arg)->getBool());
  delete (yyvsp[(4) - (6)].argVec); free((yyvsp[(3) - (6)].sValue));
;}
    break;

  case 41:
#line 374 "src/flatzinc/flatzinc.yy"
    {
  ParserState* const pp = static_cast<ParserState*>(parm);
  orfz_assert(pp, (yyvsp[(8) - (8)].arg)->isSet(), "Invalid set initializer.");
  AstSetLit* set = (yyvsp[(8) - (8)].arg)->getSet();
  pp->set_map_.put((yyvsp[(5) - (8)].sValue), *set);
  delete set;
  delete (yyvsp[(6) - (8)].argVec); free((yyvsp[(5) - (8)].sValue));
;}
    break;

  case 42:
#line 384 "src/flatzinc/flatzinc.yy"
    {
  ParserState* const pp = static_cast<ParserState*>(parm);
  orfz_assert(pp, (yyvsp[(3) - (13)].iValue)==1, "Arrays must start at 1");
  if (!pp->hadError) {
    bool print = (yyvsp[(12) - (13)].argVec)->hasCall("output_array");
    std::vector<int64> vars((yyvsp[(5) - (13)].iValue));
    if (!pp->hadError) {
      if ((yyvsp[(13) - (13)].oIntVarSpecVec).defined()) {
        std::vector<IntVarSpec*>* vsv = (yyvsp[(13) - (13)].oIntVarSpecVec).value();
        orfz_assert(pp, vsv->size() == static_cast<unsigned int>((yyvsp[(5) - (13)].iValue)),
                 "Initializer size does not match array dimension");
        if (!pp->hadError) {
          for (int i=0; i<(yyvsp[(5) - (13)].iValue); i++) {
            IntVarSpec* ivsv = (*vsv)[i];
            if (ivsv->alias) {
              vars[i] = ivsv->i;
              if ((yyvsp[(9) - (13)].oSet).defined()) {
                pp->AddIntVarDomainConstraint(ivsv->i,
                                              new AstSetLit(*(yyvsp[(9) - (13)].oSet).value()));
              }
            } else {
              vars[i] = pp->int_variables_.size();
              ivsv->SetName((yyvsp[(11) - (13)].sValue));
              pp->int_variables_.push_back(ivsv);
            }
          }
        }
        delete vsv;
      } else {
        if ((yyvsp[(5) - (13)].iValue) > 0) {
          for (int i = 0; i < (yyvsp[(5) - (13)].iValue) - 1; i++) {
            vars[i] = pp->int_variables_.size();
            const string var_name = StringPrintf("%s[%d]", (yyvsp[(11) - (13)].sValue), i + 1);
            if ((yyvsp[(9) - (13)].oSet).defined()) {
              Option<AstSetLit*> copy =
                  Option<AstSetLit*>::some((yyvsp[(9) - (13)].oSet).value()->Copy());
              pp->int_variables_.push_back(
                  new IntVarSpec(var_name, copy, false, true));
            } else {
              pp->int_variables_.push_back(
                  new IntVarSpec(var_name, (yyvsp[(9) - (13)].oSet), false, true));
            }
          }
          vars[(yyvsp[(5) - (13)].iValue) - 1] = pp->int_variables_.size();
          const string var_name =
              StringPrintf("%s[%" GG_LL_FORMAT "d]", (yyvsp[(11) - (13)].sValue), (yyvsp[(5) - (13)].iValue));
          pp->int_variables_.push_back(
              new IntVarSpec(var_name, (yyvsp[(9) - (13)].oSet), false, true));
        }
      }
    }
    if (print) {
      AstArray* a = new AstArray();
      a->a.push_back(ArrayOutput((yyvsp[(12) - (13)].argVec)->getCall("output_array")));
      AstArray* output = new AstArray();
      for (int i=0; i<(yyvsp[(5) - (13)].iValue); i++)
        output->a.push_back(new AstIntVar(vars[i]));
      a->a.push_back(output);
      a->a.push_back(new AstString(")"));
      pp->output(string((yyvsp[(11) - (13)].sValue)), a);
    }
    pp->int_var_array_map_.put((yyvsp[(11) - (13)].sValue), vars);
  }
  delete (yyvsp[(12) - (13)].argVec); free((yyvsp[(11) - (13)].sValue));
;}
    break;

  case 43:
#line 451 "src/flatzinc/flatzinc.yy"
    {
  ParserState* const pp = static_cast<ParserState*>(parm);
  bool print = (yyvsp[(12) - (13)].argVec)->hasCall("output_array");
  orfz_assert(pp, (yyvsp[(3) - (13)].iValue)==1, "Arrays must start at 1");
  if (!pp->hadError) {
    std::vector<int64> vars((yyvsp[(5) - (13)].iValue));
    if ((yyvsp[(13) - (13)].oBoolVarSpecVec).defined()) {
      std::vector<BoolVarSpec*>* vsv = (yyvsp[(13) - (13)].oBoolVarSpecVec).value();
      orfz_assert(pp, vsv->size() == static_cast<unsigned int>((yyvsp[(5) - (13)].iValue)),
               "Initializer size does not match array dimension");
      if (!pp->hadError) {
        for (int i = 0; i < (yyvsp[(5) - (13)].iValue); i++) {
          BoolVarSpec* const bvsv = (*vsv)[i];
          if (bvsv->alias)
            vars[i] = bvsv->i;
          else {
            vars[i] = pp->bool_variables_.size();
            (*vsv)[i]->SetName((yyvsp[(11) - (13)].sValue));
            pp->bool_variables_.push_back((*vsv)[i]);
          }
          if (!pp->hadError && (yyvsp[(9) - (13)].oSet).defined()) {
            AstSetLit* const domain = new AstSetLit(*(yyvsp[(9) - (13)].oSet).value());
            pp->AddBoolVarDomainConstraint(vars[i], domain);
          }
        }
      }
      delete vsv;
    } else {
      for (int i = 0; i < (yyvsp[(5) - (13)].iValue); i++) {
        vars[i] = pp->bool_variables_.size();
        pp->bool_variables_.push_back(
            new BoolVarSpec((yyvsp[(11) - (13)].sValue), (yyvsp[(9) - (13)].oSet), !print, (i == (yyvsp[(5) - (13)].iValue) - 1)));
      }
    }
    if (print) {
      AstArray* a = new AstArray();
      a->a.push_back(ArrayOutput((yyvsp[(12) - (13)].argVec)->getCall("output_array")));
      AstArray* output = new AstArray();
      for (int i=0; i<(yyvsp[(5) - (13)].iValue); i++)
        output->a.push_back(new AstBoolVar(vars[i]));
      a->a.push_back(output);
      a->a.push_back(new AstString(")"));
      pp->output(string((yyvsp[(11) - (13)].sValue)), a);
    }
    pp->bool_var_array_map_.put((yyvsp[(11) - (13)].sValue), vars);
  }
  delete (yyvsp[(12) - (13)].argVec); free((yyvsp[(11) - (13)].sValue));
;}
    break;

  case 44:
#line 501 "src/flatzinc/flatzinc.yy"
    {
  ParserState* const pp = static_cast<ParserState*>(parm);
  orfz_assert(pp, false, "Floats not supported.");
  delete (yyvsp[(12) - (13)].argVec); free((yyvsp[(11) - (13)].sValue));
;}
    break;

  case 45:
#line 508 "src/flatzinc/flatzinc.yy"
    {
  ParserState* const pp = static_cast<ParserState*>(parm);
  bool print = (yyvsp[(14) - (15)].argVec)->hasCall("output_array");
  orfz_assert(pp, (yyvsp[(3) - (15)].iValue)==1, "Arrays must start at 1");
  if (!pp->hadError) {
    std::vector<int64> vars((yyvsp[(5) - (15)].iValue));
    if ((yyvsp[(15) - (15)].oSetVarSpecVec).defined()) {
      std::vector<SetVarSpec*>* vsv = (yyvsp[(15) - (15)].oSetVarSpecVec).value();
      orfz_assert(pp, vsv->size() == static_cast<unsigned int>((yyvsp[(5) - (15)].iValue)),
               "Initializer size does not match array dimension");
      if (!pp->hadError) {
        for (int i=0; i<(yyvsp[(5) - (15)].iValue); i++) {
          SetVarSpec* svsv = (*vsv)[i];
          if (svsv->alias)
            vars[i] = svsv->i;
          else {
            vars[i] = pp->set_variables_.size();
            (*vsv)[i]->SetName((yyvsp[(13) - (15)].sValue));
            pp->set_variables_.push_back((*vsv)[i]);
          }
          if (!pp->hadError && (yyvsp[(11) - (15)].oSet).defined()) {
            AstSetLit* const domain = new AstSetLit(*(yyvsp[(11) - (15)].oSet).value());
            pp->AddSetVarDomainConstraint(vars[i], domain);
          }
        }
      }
      delete vsv;
    } else {
      if ((yyvsp[(5) - (15)].iValue)>0) {
        string arrayname = "["; arrayname += (yyvsp[(13) - (15)].sValue);
        for (int i=0; i<(yyvsp[(5) - (15)].iValue)-1; i++) {
          vars[i] = pp->set_variables_.size();
          pp->set_variables_.push_back(
              new SetVarSpec(arrayname, (yyvsp[(11) - (15)].oSet), !print, false));
        }
        vars[(yyvsp[(5) - (15)].iValue)-1] = pp->set_variables_.size();
        pp->set_variables_.push_back(new SetVarSpec((yyvsp[(13) - (15)].sValue), (yyvsp[(11) - (15)].oSet), !print, true));
      }
    }
    if (print) {
      AstArray* a = new AstArray();
      a->a.push_back(ArrayOutput((yyvsp[(14) - (15)].argVec)->getCall("output_array")));
      AstArray* output = new AstArray();
      for (int i=0; i<(yyvsp[(5) - (15)].iValue); i++)
        output->a.push_back(new AstSetVar(vars[i]));
      a->a.push_back(output);
      a->a.push_back(new AstString(")"));
      pp->output(string((yyvsp[(13) - (15)].sValue)), a);
    }
    pp->set_var_array_map_.put((yyvsp[(13) - (15)].sValue), vars);
  }
  delete (yyvsp[(14) - (15)].argVec); free((yyvsp[(13) - (15)].sValue));
;}
    break;

  case 46:
#line 563 "src/flatzinc/flatzinc.yy"
    {
  ParserState* const pp = static_cast<ParserState*>(parm);
  orfz_assert(pp, (yyvsp[(3) - (15)].iValue)==1, "Arrays must start at 1");
  orfz_assert(pp, (yyvsp[(14) - (15)].setValue)->size() == static_cast<unsigned int>((yyvsp[(5) - (15)].iValue)),
           "Initializer size does not match array dimension");
  if (!pp->hadError)
    pp->int_value_array_map_.put((yyvsp[(10) - (15)].sValue), *(yyvsp[(14) - (15)].setValue));
  delete (yyvsp[(14) - (15)].setValue);
  free((yyvsp[(10) - (15)].sValue));
  delete (yyvsp[(11) - (15)].argVec);
;}
    break;

  case 47:
#line 576 "src/flatzinc/flatzinc.yy"
    {
  ParserState* const pp = static_cast<ParserState*>(parm);
  orfz_assert(pp, (yyvsp[(3) - (15)].iValue)==1, "Arrays must start at 1");
  orfz_assert(pp, (yyvsp[(14) - (15)].setValue)->size() == static_cast<unsigned int>((yyvsp[(5) - (15)].iValue)),
           "Initializer size does not match array dimension");
  if (!pp->hadError)
    pp->bool_value_array_map_.put((yyvsp[(10) - (15)].sValue), *(yyvsp[(14) - (15)].setValue));
  delete (yyvsp[(14) - (15)].setValue);
  free((yyvsp[(10) - (15)].sValue));
  delete (yyvsp[(11) - (15)].argVec);
;}
    break;

  case 48:
#line 589 "src/flatzinc/flatzinc.yy"
    {
  ParserState* const pp = static_cast<ParserState*>(parm);
  orfz_assert(pp, false, "Floats not supported.");
  delete (yyvsp[(11) - (15)].argVec); free((yyvsp[(10) - (15)].sValue));
;}
    break;

  case 49:
#line 596 "src/flatzinc/flatzinc.yy"
    {
  ParserState* const pp = static_cast<ParserState*>(parm);
  orfz_assert(pp, (yyvsp[(3) - (17)].iValue)==1, "Arrays must start at 1");
  orfz_assert(pp, (yyvsp[(16) - (17)].setValueList)->size() == static_cast<unsigned int>((yyvsp[(5) - (17)].iValue)),
           "Initializer size does not match array dimension");
  if (!pp->hadError)
    pp->set_value_array_map_.put((yyvsp[(12) - (17)].sValue), *(yyvsp[(16) - (17)].setValueList));
  delete (yyvsp[(16) - (17)].setValueList);
  delete (yyvsp[(13) - (17)].argVec); free((yyvsp[(12) - (17)].sValue));
;}
    break;

  case 50:
#line 609 "src/flatzinc/flatzinc.yy"
    {
  (yyval.varIntSpec) = new IntVarSpec("", (yyvsp[(1) - (1)].iValue), false);
;}
    break;

  case 51:
#line 613 "src/flatzinc/flatzinc.yy"
    {
  int64 v = 0;
  ParserState* const pp = static_cast<ParserState*>(parm);
  if (pp->int_var_map_.get((yyvsp[(1) - (1)].sValue), v))
    (yyval.varIntSpec) = new IntVarSpec("", Alias(v), false);
  else {
    LOG(ERROR) << "Error: undefined identifier " << (yyvsp[(1) - (1)].sValue)
               << " in line no. " << orfz_get_lineno(pp->yyscanner);
    pp->hadError = true;
    (yyval.varIntSpec) = new IntVarSpec("", 0, false); // keep things consistent
  }
  free((yyvsp[(1) - (1)].sValue));
;}
    break;

  case 52:
#line 627 "src/flatzinc/flatzinc.yy"
    {
  std::vector<int64> v;
  ParserState* const pp = static_cast<ParserState*>(parm);
  if (pp->int_var_array_map_.get((yyvsp[(1) - (4)].sValue), v)) {
    orfz_assert(pp,static_cast<unsigned int>((yyvsp[(3) - (4)].iValue)) > 0 &&
             static_cast<unsigned int>((yyvsp[(3) - (4)].iValue)) <= v.size(),
             "array access out of bounds");
    if (!pp->hadError)
      (yyval.varIntSpec) = new IntVarSpec((yyvsp[(1) - (4)].sValue), Alias(v[(yyvsp[(3) - (4)].iValue) - 1]),false);
    else
      (yyval.varIntSpec) = new IntVarSpec((yyvsp[(1) - (4)].sValue), 0,false); // keep things consistent
  } else {
    LOG(ERROR) << "Error: undefined array identifier " << (yyvsp[(1) - (4)].sValue)
               << " in line no. " << orfz_get_lineno(pp->yyscanner);
    pp->hadError = true;
    (yyval.varIntSpec) = new IntVarSpec((yyvsp[(1) - (4)].sValue), 0,false); // keep things consistent
  }
  free((yyvsp[(1) - (4)].sValue));
;}
    break;

  case 53:
#line 649 "src/flatzinc/flatzinc.yy"
    { (yyval.varIntSpecVec) = new std::vector<IntVarSpec*>(0); ;}
    break;

  case 54:
#line 651 "src/flatzinc/flatzinc.yy"
    { (yyval.varIntSpecVec) = (yyvsp[(1) - (2)].varIntSpecVec); ;}
    break;

  case 55:
#line 655 "src/flatzinc/flatzinc.yy"
    { (yyval.varIntSpecVec) = new std::vector<IntVarSpec*>(1); (*(yyval.varIntSpecVec))[0] = (yyvsp[(1) - (1)].varIntSpec); ;}
    break;

  case 56:
#line 657 "src/flatzinc/flatzinc.yy"
    { (yyval.varIntSpecVec) = (yyvsp[(1) - (3)].varIntSpecVec); (yyval.varIntSpecVec)->push_back((yyvsp[(3) - (3)].varIntSpec)); ;}
    break;

  case 59:
#line 662 "src/flatzinc/flatzinc.yy"
    { (yyval.varIntSpecVec) = (yyvsp[(2) - (3)].varIntSpecVec); ;}
    break;

  case 60:
#line 666 "src/flatzinc/flatzinc.yy"
    { (yyval.varFloatSpec) = new FloatVarSpec("", (yyvsp[(1) - (1)].dValue),false); ;}
    break;

  case 61:
#line 668 "src/flatzinc/flatzinc.yy"
    {
  int64 v = 0;
  ParserState* const pp = static_cast<ParserState*>(parm);
  if (pp->float_var_map_.get((yyvsp[(1) - (1)].sValue), v))
    (yyval.varFloatSpec) = new FloatVarSpec("", Alias(v),false);
  else {
    LOG(ERROR) << "Error: undefined identifier " << (yyvsp[(1) - (1)].sValue)
               << " in line no. " << orfz_get_lineno(pp->yyscanner);
    pp->hadError = true;
    (yyval.varFloatSpec) = new FloatVarSpec("", 0.0,false);
  }
  free((yyvsp[(1) - (1)].sValue));
;}
    break;

  case 62:
#line 682 "src/flatzinc/flatzinc.yy"
    {
  std::vector<int64> v;
  ParserState* const pp = static_cast<ParserState*>(parm);
  if (pp->float_var_array_map_.get((yyvsp[(1) - (4)].sValue), v)) {
    orfz_assert(pp,static_cast<unsigned int>((yyvsp[(3) - (4)].iValue)) > 0 &&
             static_cast<unsigned int>((yyvsp[(3) - (4)].iValue)) <= v.size(),
             "array access out of bounds");
    if (!pp->hadError)
      (yyval.varFloatSpec) = new FloatVarSpec((yyvsp[(1) - (4)].sValue), Alias(v[(yyvsp[(3) - (4)].iValue)-1]),false);
    else
      (yyval.varFloatSpec) = new FloatVarSpec((yyvsp[(1) - (4)].sValue), 0.0,false);
  } else {
    LOG(ERROR) << "Error: undefined array identifier " << (yyvsp[(1) - (4)].sValue)
               << " in line no. " << orfz_get_lineno(pp->yyscanner);
    pp->hadError = true;
    (yyval.varFloatSpec) = new FloatVarSpec((yyvsp[(1) - (4)].sValue), 0.0,false);
  }
  free((yyvsp[(1) - (4)].sValue));
;}
    break;

  case 63:
#line 704 "src/flatzinc/flatzinc.yy"
    { (yyval.varFloatSpecVec) = new std::vector<FloatVarSpec*>(0); ;}
    break;

  case 64:
#line 706 "src/flatzinc/flatzinc.yy"
    { (yyval.varFloatSpecVec) = (yyvsp[(1) - (2)].varFloatSpecVec); ;}
    break;

  case 65:
#line 710 "src/flatzinc/flatzinc.yy"
    { (yyval.varFloatSpecVec) = new std::vector<FloatVarSpec*>(1); (*(yyval.varFloatSpecVec))[0] = (yyvsp[(1) - (1)].varFloatSpec); ;}
    break;

  case 66:
#line 712 "src/flatzinc/flatzinc.yy"
    { (yyval.varFloatSpecVec) = (yyvsp[(1) - (3)].varFloatSpecVec); (yyval.varFloatSpecVec)->push_back((yyvsp[(3) - (3)].varFloatSpec)); ;}
    break;

  case 67:
#line 716 "src/flatzinc/flatzinc.yy"
    { (yyval.varFloatSpecVec) = (yyvsp[(2) - (3)].varFloatSpecVec); ;}
    break;

  case 68:
#line 720 "src/flatzinc/flatzinc.yy"
    { (yyval.varBoolSpec) = new BoolVarSpec("", (yyvsp[(1) - (1)].iValue),false); ;}
    break;

  case 69:
#line 722 "src/flatzinc/flatzinc.yy"
    {
  int64 v = 0;
  ParserState* const pp = static_cast<ParserState*>(parm);
  if (pp->bool_var_map_.get((yyvsp[(1) - (1)].sValue), v))
    (yyval.varBoolSpec) = new BoolVarSpec("", Alias(v),false);
  else {
    LOG(ERROR) << "Error: undefined identifier " << (yyvsp[(1) - (1)].sValue)
               << " in line no. " << orfz_get_lineno(pp->yyscanner);
    pp->hadError = true;
    (yyval.varBoolSpec) = new BoolVarSpec("", false,false);
  }
  free((yyvsp[(1) - (1)].sValue));
;}
    break;

  case 70:
#line 736 "src/flatzinc/flatzinc.yy"
    {
  std::vector<int64> v;
  ParserState* const pp = static_cast<ParserState*>(parm);
  if (pp->bool_var_array_map_.get((yyvsp[(1) - (4)].sValue), v)) {
    orfz_assert(pp,static_cast<unsigned int>((yyvsp[(3) - (4)].iValue)) > 0 &&
             static_cast<unsigned int>((yyvsp[(3) - (4)].iValue)) <= v.size(),
             "array access out of bounds");
    if (!pp->hadError)
      (yyval.varBoolSpec) = new BoolVarSpec((yyvsp[(1) - (4)].sValue), Alias(v[(yyvsp[(3) - (4)].iValue)-1]),false);
    else
      (yyval.varBoolSpec) = new BoolVarSpec((yyvsp[(1) - (4)].sValue), false,false);
  } else {
    LOG(ERROR) << "Error: undefined array identifier " << (yyvsp[(1) - (4)].sValue)
               << " in line no. " << orfz_get_lineno(pp->yyscanner);
    pp->hadError = true;
    (yyval.varBoolSpec) = new BoolVarSpec((yyvsp[(1) - (4)].sValue), false,false);
  }
  free((yyvsp[(1) - (4)].sValue));
;}
    break;

  case 71:
#line 758 "src/flatzinc/flatzinc.yy"
    { (yyval.varBoolSpecVec) = new std::vector<BoolVarSpec*>(0); ;}
    break;

  case 72:
#line 760 "src/flatzinc/flatzinc.yy"
    { (yyval.varBoolSpecVec) = (yyvsp[(1) - (2)].varBoolSpecVec); ;}
    break;

  case 73:
#line 764 "src/flatzinc/flatzinc.yy"
    { (yyval.varBoolSpecVec) = new std::vector<BoolVarSpec*>(1); (*(yyval.varBoolSpecVec))[0] = (yyvsp[(1) - (1)].varBoolSpec); ;}
    break;

  case 74:
#line 766 "src/flatzinc/flatzinc.yy"
    { (yyval.varBoolSpecVec) = (yyvsp[(1) - (3)].varBoolSpecVec); (yyval.varBoolSpecVec)->push_back((yyvsp[(3) - (3)].varBoolSpec)); ;}
    break;

  case 75:
#line 768 "src/flatzinc/flatzinc.yy"
    { (yyval.varBoolSpecVec) = (yyvsp[(2) - (3)].varBoolSpecVec); ;}
    break;

  case 76:
#line 772 "src/flatzinc/flatzinc.yy"
    { (yyval.varSetSpec) = new SetVarSpec("", (yyvsp[(1) - (1)].setLit),false); ;}
    break;

  case 77:
#line 774 "src/flatzinc/flatzinc.yy"
    {
  ParserState* const pp = static_cast<ParserState*>(parm);
  int64 v = 0;
  if (pp->set_var_map_.get((yyvsp[(1) - (1)].sValue), v)) {
    (yyval.varSetSpec) = new SetVarSpec("", Alias(v),false);
  } else {
    LOG(ERROR) << "Error: undefined identifier " << (yyvsp[(1) - (1)].sValue)
               << " in line no. " << orfz_get_lineno(pp->yyscanner);
    pp->hadError = true;
    (yyval.varSetSpec) = new SetVarSpec("", Alias(0),false);
  }
  free((yyvsp[(1) - (1)].sValue));
;}
    break;

  case 78:
#line 788 "src/flatzinc/flatzinc.yy"
    {
  std::vector<int64> v;
  ParserState* const pp = static_cast<ParserState*>(parm);
  if (pp->set_var_array_map_.get((yyvsp[(1) - (4)].sValue), v)) {
    orfz_assert(pp,static_cast<unsigned int>((yyvsp[(3) - (4)].iValue)) > 0 &&
             static_cast<unsigned int>((yyvsp[(3) - (4)].iValue)) <= v.size(),
             "array access out of bounds");
    if (!pp->hadError)
      (yyval.varSetSpec) = new SetVarSpec((yyvsp[(1) - (4)].sValue), Alias(v[(yyvsp[(3) - (4)].iValue)-1]),false);
    else
      (yyval.varSetSpec) = new SetVarSpec((yyvsp[(1) - (4)].sValue), Alias(0),false);
  } else {
    LOG(ERROR) << "Error: undefined array identifier " << (yyvsp[(1) - (4)].sValue)
               << " in line no. " << orfz_get_lineno(pp->yyscanner);
    pp->hadError = true;
    (yyval.varSetSpec) = new SetVarSpec((yyvsp[(1) - (4)].sValue), Alias(0),false);
  }
  free((yyvsp[(1) - (4)].sValue));
;}
    break;

  case 79:
#line 810 "src/flatzinc/flatzinc.yy"
    { (yyval.varSetSpecVec) = new std::vector<SetVarSpec*>(0); ;}
    break;

  case 80:
#line 812 "src/flatzinc/flatzinc.yy"
    { (yyval.varSetSpecVec) = (yyvsp[(1) - (2)].varSetSpecVec); ;}
    break;

  case 81:
#line 816 "src/flatzinc/flatzinc.yy"
    { (yyval.varSetSpecVec) = new std::vector<SetVarSpec*>(1); (*(yyval.varSetSpecVec))[0] = (yyvsp[(1) - (1)].varSetSpec); ;}
    break;

  case 82:
#line 818 "src/flatzinc/flatzinc.yy"
    { (yyval.varSetSpecVec) = (yyvsp[(1) - (3)].varSetSpecVec); (yyval.varSetSpecVec)->push_back((yyvsp[(3) - (3)].varSetSpec)); ;}
    break;

  case 83:
#line 821 "src/flatzinc/flatzinc.yy"
    { (yyval.varSetSpecVec) = (yyvsp[(2) - (3)].varSetSpecVec); ;}
    break;

  case 84:
#line 825 "src/flatzinc/flatzinc.yy"
    { (yyval.oIntVarSpecVec) = Option<std::vector<IntVarSpec*>*>::none(); ;}
    break;

  case 85:
#line 827 "src/flatzinc/flatzinc.yy"
    { (yyval.oIntVarSpecVec) = Option<std::vector<IntVarSpec*>*>::some((yyvsp[(2) - (2)].varIntSpecVec)); ;}
    break;

  case 86:
#line 831 "src/flatzinc/flatzinc.yy"
    { (yyval.oBoolVarSpecVec) = Option<std::vector<BoolVarSpec*>*>::none(); ;}
    break;

  case 87:
#line 833 "src/flatzinc/flatzinc.yy"
    { (yyval.oBoolVarSpecVec) = Option<std::vector<BoolVarSpec*>*>::some((yyvsp[(2) - (2)].varBoolSpecVec)); ;}
    break;

  case 88:
#line 837 "src/flatzinc/flatzinc.yy"
    { (yyval.oFloatVarSpecVec) = Option<std::vector<FloatVarSpec*>*>::none(); ;}
    break;

  case 89:
#line 839 "src/flatzinc/flatzinc.yy"
    { (yyval.oFloatVarSpecVec) = Option<std::vector<FloatVarSpec*>*>::some((yyvsp[(2) - (2)].varFloatSpecVec)); ;}
    break;

  case 90:
#line 843 "src/flatzinc/flatzinc.yy"
    { (yyval.oSetVarSpecVec) = Option<std::vector<SetVarSpec*>*>::none(); ;}
    break;

  case 91:
#line 845 "src/flatzinc/flatzinc.yy"
    { (yyval.oSetVarSpecVec) = Option<std::vector<SetVarSpec*>*>::some((yyvsp[(2) - (2)].varSetSpecVec)); ;}
    break;

  case 92:
#line 849 "src/flatzinc/flatzinc.yy"
    {
  ParserState* const pp = static_cast<ParserState*>(parm);
  if (!pp->hadError) {
    try {
      pp->AddConstraint((yyvsp[(2) - (6)].sValue), (yyvsp[(4) - (6)].argVec), (yyvsp[(6) - (6)].argVec));
    } catch (operations_research::Error& e) {
      yyerror(pp, e.DebugString().c_str());
    }
  }
  free((yyvsp[(2) - (6)].sValue));
;}
    break;

  case 93:
#line 862 "src/flatzinc/flatzinc.yy"
    {
  ParserState* const pp = static_cast<ParserState*>(parm);
  if (!pp->hadError) {
    try {
      pp->AnalyseAndCreateModel();
      pp->model()->Satisfy((yyvsp[(2) - (3)].argVec));
    } catch (operations_research::Error& e) {
      yyerror(pp, e.DebugString().c_str());
    }
  } else {
    delete (yyvsp[(2) - (3)].argVec);
  }
;}
    break;

  case 94:
#line 876 "src/flatzinc/flatzinc.yy"
    {
  ParserState* const pp = static_cast<ParserState*>(parm);
  if (!pp->hadError) {
    try {
      pp->AnalyseAndCreateModel();
      if ((yyvsp[(3) - (4)].bValue))
        pp->model()->Minimize((yyvsp[(4) - (4)].iValue),(yyvsp[(2) - (4)].argVec));
      else
        pp->model()->Maximize((yyvsp[(4) - (4)].iValue),(yyvsp[(2) - (4)].argVec));
    } catch (operations_research::Error& e) {
      yyerror(pp, e.DebugString().c_str());
    }
  } else {
    delete (yyvsp[(2) - (4)].argVec);
  }
;}
    break;

  case 95:
#line 899 "src/flatzinc/flatzinc.yy"
    { (yyval.oSet) = Option<AstSetLit*>::none(); ;}
    break;

  case 96:
#line 901 "src/flatzinc/flatzinc.yy"
    { (yyval.oSet) = Option<AstSetLit*>::some(new AstSetLit(*(yyvsp[(2) - (3)].setValue))); ;}
    break;

  case 97:
#line 903 "src/flatzinc/flatzinc.yy"
    {
  (yyval.oSet) = Option<AstSetLit*>::some(new AstSetLit((yyvsp[(1) - (3)].iValue), (yyvsp[(3) - (3)].iValue)));
;}
    break;

  case 98:
#line 909 "src/flatzinc/flatzinc.yy"
    { (yyval.oSet) = Option<AstSetLit*>::none(); ;}
    break;

  case 99:
#line 911 "src/flatzinc/flatzinc.yy"
    { bool haveTrue = false;
  bool haveFalse = false;
  for (int i=(yyvsp[(2) - (4)].setValue)->size(); i--;) {
    haveTrue |= ((*(yyvsp[(2) - (4)].setValue))[i] == 1);
    haveFalse |= ((*(yyvsp[(2) - (4)].setValue))[i] == 0);
  }
  delete (yyvsp[(2) - (4)].setValue);
  (yyval.oSet) = Option<AstSetLit*>::some(
      new AstSetLit(!haveFalse,haveTrue));
;}
    break;

  case 102:
#line 932 "src/flatzinc/flatzinc.yy"
    { (yyval.setLit) = new AstSetLit(*(yyvsp[(2) - (3)].setValue)); ;}
    break;

  case 103:
#line 934 "src/flatzinc/flatzinc.yy"
    { (yyval.setLit) = new AstSetLit((yyvsp[(1) - (3)].iValue), (yyvsp[(3) - (3)].iValue)); ;}
    break;

  case 104:
#line 940 "src/flatzinc/flatzinc.yy"
    { (yyval.setValue) = new std::vector<int64>(0); ;}
    break;

  case 105:
#line 942 "src/flatzinc/flatzinc.yy"
    { (yyval.setValue) = (yyvsp[(1) - (2)].setValue); ;}
    break;

  case 106:
#line 946 "src/flatzinc/flatzinc.yy"
    { (yyval.setValue) = new std::vector<int64>(1); (*(yyval.setValue))[0] = (yyvsp[(1) - (1)].iValue); ;}
    break;

  case 107:
#line 948 "src/flatzinc/flatzinc.yy"
    { (yyval.setValue) = (yyvsp[(1) - (3)].setValue); (yyval.setValue)->push_back((yyvsp[(3) - (3)].iValue)); ;}
    break;

  case 108:
#line 952 "src/flatzinc/flatzinc.yy"
    { (yyval.setValue) = new std::vector<int64>(0); ;}
    break;

  case 109:
#line 954 "src/flatzinc/flatzinc.yy"
    { (yyval.setValue) = (yyvsp[(1) - (2)].setValue); ;}
    break;

  case 110:
#line 958 "src/flatzinc/flatzinc.yy"
    { (yyval.setValue) = new std::vector<int64>(1); (*(yyval.setValue))[0] = (yyvsp[(1) - (1)].iValue); ;}
    break;

  case 111:
#line 960 "src/flatzinc/flatzinc.yy"
    { (yyval.setValue) = (yyvsp[(1) - (3)].setValue); (yyval.setValue)->push_back((yyvsp[(3) - (3)].iValue)); ;}
    break;

  case 112:
#line 964 "src/flatzinc/flatzinc.yy"
    { (yyval.floatSetValue) = new std::vector<double>(0); ;}
    break;

  case 113:
#line 966 "src/flatzinc/flatzinc.yy"
    { (yyval.floatSetValue) = (yyvsp[(1) - (2)].floatSetValue); ;}
    break;

  case 114:
#line 970 "src/flatzinc/flatzinc.yy"
    { (yyval.floatSetValue) = new std::vector<double>(1); (*(yyval.floatSetValue))[0] = (yyvsp[(1) - (1)].dValue); ;}
    break;

  case 115:
#line 972 "src/flatzinc/flatzinc.yy"
    { (yyval.floatSetValue) = (yyvsp[(1) - (3)].floatSetValue); (yyval.floatSetValue)->push_back((yyvsp[(3) - (3)].dValue)); ;}
    break;

  case 116:
#line 976 "src/flatzinc/flatzinc.yy"
    { (yyval.setValueList) = new std::vector<AstSetLit>(0); ;}
    break;

  case 117:
#line 978 "src/flatzinc/flatzinc.yy"
    { (yyval.setValueList) = (yyvsp[(1) - (2)].setValueList); ;}
    break;

  case 118:
#line 982 "src/flatzinc/flatzinc.yy"
    { (yyval.setValueList) = new std::vector<AstSetLit>(1); (*(yyval.setValueList))[0] = *(yyvsp[(1) - (1)].setLit); delete (yyvsp[(1) - (1)].setLit); ;}
    break;

  case 119:
#line 984 "src/flatzinc/flatzinc.yy"
    { (yyval.setValueList) = (yyvsp[(1) - (3)].setValueList); (yyval.setValueList)->push_back(*(yyvsp[(3) - (3)].setLit)); delete (yyvsp[(3) - (3)].setLit); ;}
    break;

  case 120:
#line 992 "src/flatzinc/flatzinc.yy"
    { (yyval.argVec) = new AstArray((yyvsp[(1) - (1)].arg)); ;}
    break;

  case 121:
#line 994 "src/flatzinc/flatzinc.yy"
    { (yyval.argVec) = (yyvsp[(1) - (3)].argVec); (yyval.argVec)->append((yyvsp[(3) - (3)].arg)); ;}
    break;

  case 122:
#line 998 "src/flatzinc/flatzinc.yy"
    { (yyval.arg) = (yyvsp[(1) - (1)].arg); ;}
    break;

  case 123:
#line 1000 "src/flatzinc/flatzinc.yy"
    { (yyval.arg) = (yyvsp[(2) - (3)].argVec); ;}
    break;

  case 124:
#line 1004 "src/flatzinc/flatzinc.yy"
    { (yyval.oArg) = Option<AstNode*>::none(); ;}
    break;

  case 125:
#line 1006 "src/flatzinc/flatzinc.yy"
    { (yyval.oArg) = Option<AstNode*>::some((yyvsp[(2) - (2)].arg)); ;}
    break;

  case 126:
#line 1010 "src/flatzinc/flatzinc.yy"
    { (yyval.arg) = new AstBoolLit((yyvsp[(1) - (1)].iValue)); ;}
    break;

  case 127:
#line 1012 "src/flatzinc/flatzinc.yy"
    { (yyval.arg) = new AstIntLit((yyvsp[(1) - (1)].iValue)); ;}
    break;

  case 128:
#line 1014 "src/flatzinc/flatzinc.yy"
    { (yyval.arg) = new AstFloatLit((yyvsp[(1) - (1)].dValue)); ;}
    break;

  case 129:
#line 1016 "src/flatzinc/flatzinc.yy"
    { (yyval.arg) = (yyvsp[(1) - (1)].setLit); ;}
    break;

  case 130:
#line 1018 "src/flatzinc/flatzinc.yy"
    {
  std::vector<int64> as;
  ParserState* const pp = static_cast<ParserState*>(parm);
  if (pp->int_var_array_map_.get((yyvsp[(1) - (1)].sValue), as)) {
    AstArray* ia = new AstArray(as.size());
    for (int i=as.size(); i--;)
      ia->a[i] = new AstIntVar(as[i]);
    (yyval.arg) = ia;
  } else if (pp->bool_var_array_map_.get((yyvsp[(1) - (1)].sValue), as)) {
    AstArray* ia = new AstArray(as.size());
    for (int i=as.size(); i--;)
      ia->a[i] = new AstBoolVar(as[i]);
    (yyval.arg) = ia;
  } else if (pp->set_var_array_map_.get((yyvsp[(1) - (1)].sValue), as)) {
    AstArray* ia = new AstArray(as.size());
    for (int i=as.size(); i--;)
      ia->a[i] = new AstSetVar(as[i]);
    (yyval.arg) = ia;
  } else {
    std::vector<int64> is;
    std::vector<AstSetLit> isS;
    int64 ival = 0;
    bool bval = false;
    if (pp->int_value_array_map_.get((yyvsp[(1) - (1)].sValue), is)) {
      AstArray* v = new AstArray(is.size());
      for (int i=is.size(); i--;)
        v->a[i] = new AstIntLit(is[i]);
      (yyval.arg) = v;
    } else if (pp->bool_value_array_map_.get((yyvsp[(1) - (1)].sValue), is)) {
      AstArray* v = new AstArray(is.size());
      for (int i=is.size(); i--;)
        v->a[i] = new AstBoolLit(is[i]);
      (yyval.arg) = v;
    } else if (pp->set_value_array_map_.get((yyvsp[(1) - (1)].sValue), isS)) {
      AstArray* v = new AstArray(isS.size());
      for (int i=isS.size(); i--;)
        v->a[i] = new AstSetLit(isS[i]);
      (yyval.arg) = v;
    } else if (pp->int_map_.get((yyvsp[(1) - (1)].sValue), ival)) {
      (yyval.arg) = new AstIntLit(ival);
    } else if (pp->bool_map_.get((yyvsp[(1) - (1)].sValue), bval)) {
      (yyval.arg) = new AstBoolLit(bval);
    } else {
      (yyval.arg) = pp->VarRefArg((yyvsp[(1) - (1)].sValue), false);
    }
  }
  free((yyvsp[(1) - (1)].sValue));
;}
    break;

  case 131:
#line 1067 "src/flatzinc/flatzinc.yy"
    {
  ParserState* const pp = static_cast<ParserState*>(parm);
  int i = -1;
  orfz_assert(pp, (yyvsp[(3) - (4)].arg)->isInt(i), "Non-integer array index.");
  if (!pp->hadError)
    (yyval.arg) = static_cast<ParserState*>(parm)->ArrayElement((yyvsp[(1) - (4)].sValue), i);
  else
    (yyval.arg) = new AstIntLit(0); // keep things consistent
  free((yyvsp[(1) - (4)].sValue)); delete (yyvsp[(3) - (4)].arg);
;}
    break;

  case 132:
#line 1080 "src/flatzinc/flatzinc.yy"
    { (yyval.argVec) = new AstArray(0); ;}
    break;

  case 133:
#line 1082 "src/flatzinc/flatzinc.yy"
    { (yyval.argVec) = (yyvsp[(1) - (2)].argVec); ;}
    break;

  case 134:
#line 1086 "src/flatzinc/flatzinc.yy"
    { (yyval.argVec) = new AstArray((yyvsp[(1) - (1)].arg)); ;}
    break;

  case 135:
#line 1088 "src/flatzinc/flatzinc.yy"
    { (yyval.argVec) = (yyvsp[(1) - (3)].argVec); (yyval.argVec)->append((yyvsp[(3) - (3)].arg)); ;}
    break;

  case 136:
#line 1096 "src/flatzinc/flatzinc.yy"
    {
  ParserState* const pp = static_cast<ParserState*>(parm);
  int64 value;
  if (!pp->int_var_map_.get((yyvsp[(1) - (1)].sValue), value)) {
    LOG(ERROR) << "Error: unknown integer variable " << (yyvsp[(1) - (1)].sValue)
               << " in line no. " << orfz_get_lineno(pp->yyscanner);
    pp->hadError = true;
  }
  (yyval.iValue) = value;
  free((yyvsp[(1) - (1)].sValue));
;}
    break;

  case 137:
#line 1108 "src/flatzinc/flatzinc.yy"
    {
  std::vector<int64> tmp;
  ParserState* const pp = static_cast<ParserState*>(parm);
  if (!pp->int_var_array_map_.get((yyvsp[(1) - (4)].sValue), tmp)) {
    LOG(ERROR) << "Error: unknown integer variable array " << (yyvsp[(1) - (4)].sValue)
               << " in line no. " << orfz_get_lineno(pp->yyscanner);
    pp->hadError = true;
  }
  if ((yyvsp[(3) - (4)].iValue) == 0 || static_cast<unsigned int>((yyvsp[(3) - (4)].iValue)) > tmp.size()) {
    LOG(ERROR) << "Error: array index out of bounds for array " << (yyvsp[(1) - (4)].sValue)
               << " in line no. " << orfz_get_lineno(pp->yyscanner);
    pp->hadError = true;
  } else {
    (yyval.iValue) = tmp[(yyvsp[(3) - (4)].iValue) - 1];
  }
  free((yyvsp[(1) - (4)].sValue));
;}
    break;

  case 140:
#line 1136 "src/flatzinc/flatzinc.yy"
    { (yyval.argVec) = NULL; ;}
    break;

  case 141:
#line 1138 "src/flatzinc/flatzinc.yy"
    { (yyval.argVec) = (yyvsp[(1) - (1)].argVec); ;}
    break;

  case 142:
#line 1142 "src/flatzinc/flatzinc.yy"
    { (yyval.argVec) = new AstArray((yyvsp[(2) - (2)].arg)); ;}
    break;

  case 143:
#line 1144 "src/flatzinc/flatzinc.yy"
    { (yyval.argVec) = (yyvsp[(1) - (3)].argVec); (yyval.argVec)->append((yyvsp[(3) - (3)].arg)); ;}
    break;

  case 144:
#line 1148 "src/flatzinc/flatzinc.yy"
    {
  (yyval.arg) = new AstCall((yyvsp[(1) - (4)].sValue), AstExtractSingleton((yyvsp[(3) - (4)].arg))); free((yyvsp[(1) - (4)].sValue));
;}
    break;

  case 145:
#line 1152 "src/flatzinc/flatzinc.yy"
    { (yyval.arg) = (yyvsp[(1) - (1)].arg); ;}
    break;

  case 146:
#line 1156 "src/flatzinc/flatzinc.yy"
    { (yyval.arg) = new AstArray((yyvsp[(1) - (1)].arg)); ;}
    break;

  case 147:
#line 1158 "src/flatzinc/flatzinc.yy"
    { (yyval.arg) = (yyvsp[(1) - (3)].arg); (yyval.arg)->append((yyvsp[(3) - (3)].arg)); ;}
    break;

  case 148:
#line 1162 "src/flatzinc/flatzinc.yy"
    { (yyval.arg) = (yyvsp[(1) - (1)].arg); ;}
    break;

  case 149:
#line 1164 "src/flatzinc/flatzinc.yy"
    { (yyval.arg) = (yyvsp[(2) - (3)].arg); ;}
    break;

  case 150:
#line 1168 "src/flatzinc/flatzinc.yy"
    { (yyval.arg) = new AstBoolLit((yyvsp[(1) - (1)].iValue)); ;}
    break;

  case 151:
#line 1170 "src/flatzinc/flatzinc.yy"
    { (yyval.arg) = new AstIntLit((yyvsp[(1) - (1)].iValue)); ;}
    break;

  case 152:
#line 1172 "src/flatzinc/flatzinc.yy"
    { (yyval.arg) = new AstFloatLit((yyvsp[(1) - (1)].dValue)); ;}
    break;

  case 153:
#line 1174 "src/flatzinc/flatzinc.yy"
    { (yyval.arg) = (yyvsp[(1) - (1)].setLit); ;}
    break;

  case 154:
#line 1176 "src/flatzinc/flatzinc.yy"
    {
  std::vector<int64> as;
  ParserState* const pp = static_cast<ParserState*>(parm);
  if (pp->int_var_array_map_.get((yyvsp[(1) - (1)].sValue), as)) {
    AstArray* const ia = new AstArray(as.size());
    for (int i = as.size(); i--;) {
      ia->a[i] = new AstIntVar(as[i]);
    }
    (yyval.arg) = ia;
  } else if (pp->bool_var_array_map_.get((yyvsp[(1) - (1)].sValue), as)) {
    AstArray* const ia = new AstArray(as.size());
    for (int i=as.size(); i--;) {
      ia->a[i] = new AstBoolVar(as[i]);
    }
    (yyval.arg) = ia;
  } else if (pp->set_var_array_map_.get((yyvsp[(1) - (1)].sValue), as)) {
    AstArray* const ia = new AstArray(as.size());
    for (int i = as.size(); i--;) {
      ia->a[i] = new AstSetVar(as[i]);
    }
    (yyval.arg) = ia;
  } else {
    std::vector<int64> is;
    int64 ival = 0;
    bool bval = false;
    if (pp->int_value_array_map_.get((yyvsp[(1) - (1)].sValue), is)) {
      AstArray* const v = new AstArray(is.size());
      for (int i = is.size(); i--;) {
        v->a[i] = new AstIntLit(is[i]);
      }
      (yyval.arg) = v;
    } else if (pp->bool_value_array_map_.get((yyvsp[(1) - (1)].sValue), is)) {
      AstArray* const v = new AstArray(is.size());
      for (int i = is.size(); i--;) {
        v->a[i] = new AstBoolLit(is[i]);
      }
      (yyval.arg) = v;
    } else if (pp->int_map_.get((yyvsp[(1) - (1)].sValue), ival)) {
      (yyval.arg) = new AstIntLit(ival);
    } else if (pp->bool_map_.get((yyvsp[(1) - (1)].sValue), bval)) {
      (yyval.arg) = new AstBoolLit(bval);
    } else {
      (yyval.arg) = pp->VarRefArg((yyvsp[(1) - (1)].sValue), true);
    }
  }
  free((yyvsp[(1) - (1)].sValue));
;}
    break;

  case 155:
#line 1224 "src/flatzinc/flatzinc.yy"
    {
  ParserState* const pp = static_cast<ParserState*>(parm);
  int i = -1;
  orfz_assert(pp, (yyvsp[(3) - (4)].arg)->isInt(i), "Non-integer array index.");
  if (!pp->hadError)
    (yyval.arg) = pp->ArrayElement((yyvsp[(1) - (4)].sValue), i);
  else
    (yyval.arg) = new AstIntLit(0); // keep things consistent
  free((yyvsp[(1) - (4)].sValue)); delete (yyvsp[(3) - (4)].arg);
;}
    break;

  case 156:
#line 1235 "src/flatzinc/flatzinc.yy"
    {
  (yyval.arg) = new AstString((yyvsp[(1) - (1)].sValue));
  free((yyvsp[(1) - (1)].sValue));
;}
    break;


/* Line 1267 of yacc.c.  */
#line 2998 "src/flatzinc/win/flatzinc.tab.cc"
      default: break;
    }
  YY_SYMBOL_PRINT ("-> $$ =", yyr1[yyn], &yyval, &yyloc);

  YYPOPSTACK (yylen);
  yylen = 0;
  YY_STACK_PRINT (yyss, yyssp);

  *++yyvsp = yyval;


  /* Now `shift' the result of the reduction.  Determine what state
     that goes to, based on the state we popped back to and the rule
     number reduced by.  */

  yyn = yyr1[yyn];

  yystate = yypgoto[yyn - YYNTOKENS] + *yyssp;
  if (0 <= yystate && yystate <= YYLAST && yycheck[yystate] == *yyssp)
    yystate = yytable[yystate];
  else
    yystate = yydefgoto[yyn - YYNTOKENS];

  goto yynewstate;


/*------------------------------------.
| yyerrlab -- here on detecting error |
`------------------------------------*/
yyerrlab:
  /* If not already recovering from an error, report this error.  */
  if (!yyerrstatus)
    {
      ++yynerrs;
#if ! YYERROR_VERBOSE
      yyerror (parm, YY_("syntax error"));
#else
      {
	YYSIZE_T yysize = yysyntax_error (0, yystate, yychar);
	if (yymsg_alloc < yysize && yymsg_alloc < YYSTACK_ALLOC_MAXIMUM)
	  {
	    YYSIZE_T yyalloc = 2 * yysize;
	    if (! (yysize <= yyalloc && yyalloc <= YYSTACK_ALLOC_MAXIMUM))
	      yyalloc = YYSTACK_ALLOC_MAXIMUM;
	    if (yymsg != yymsgbuf)
	      YYSTACK_FREE (yymsg);
	    yymsg = (char *) YYSTACK_ALLOC (yyalloc);
	    if (yymsg)
	      yymsg_alloc = yyalloc;
	    else
	      {
		yymsg = yymsgbuf;
		yymsg_alloc = sizeof yymsgbuf;
	      }
	  }

	if (0 < yysize && yysize <= yymsg_alloc)
	  {
	    (void) yysyntax_error (yymsg, yystate, yychar);
	    yyerror (parm, yymsg);
	  }
	else
	  {
	    yyerror (parm, YY_("syntax error"));
	    if (yysize != 0)
	      goto yyexhaustedlab;
	  }
      }
#endif
    }



  if (yyerrstatus == 3)
    {
      /* If just tried and failed to reuse look-ahead token after an
	 error, discard it.  */

      if (yychar <= YYEOF)
	{
	  /* Return failure if at end of input.  */
	  if (yychar == YYEOF)
	    YYABORT;
	}
      else
	{
	  yydestruct ("Error: discarding",
		      yytoken, &yylval, parm);
	  yychar = YYEMPTY;
	}
    }

  /* Else will try to reuse look-ahead token after shifting the error
     token.  */
  goto yyerrlab1;


/*---------------------------------------------------.
| yyerrorlab -- error raised explicitly by YYERROR.  |
`---------------------------------------------------*/
yyerrorlab:

  /* Pacify compilers like GCC when the user code never invokes
     YYERROR and the label yyerrorlab therefore never appears in user
     code.  */
  if (/*CONSTCOND*/ 0)
     goto yyerrorlab;

  /* Do not reclaim the symbols of the rule which action triggered
     this YYERROR.  */
  YYPOPSTACK (yylen);
  yylen = 0;
  YY_STACK_PRINT (yyss, yyssp);
  yystate = *yyssp;
  goto yyerrlab1;


/*-------------------------------------------------------------.
| yyerrlab1 -- common code for both syntax error and YYERROR.  |
`-------------------------------------------------------------*/
yyerrlab1:
  yyerrstatus = 3;	/* Each real token shifted decrements this.  */

  for (;;)
    {
      yyn = yypact[yystate];
      if (yyn != YYPACT_NINF)
	{
	  yyn += YYTERROR;
	  if (0 <= yyn && yyn <= YYLAST && yycheck[yyn] == YYTERROR)
	    {
	      yyn = yytable[yyn];
	      if (0 < yyn)
		break;
	    }
	}

      /* Pop the current state because it cannot handle the error token.  */
      if (yyssp == yyss)
	YYABORT;


      yydestruct ("Error: popping",
		  yystos[yystate], yyvsp, parm);
      YYPOPSTACK (1);
      yystate = *yyssp;
      YY_STACK_PRINT (yyss, yyssp);
    }

  if (yyn == YYFINAL)
    YYACCEPT;

  *++yyvsp = yylval;


  /* Shift the error token.  */
  YY_SYMBOL_PRINT ("Shifting", yystos[yyn], yyvsp, yylsp);

  yystate = yyn;
  goto yynewstate;


/*-------------------------------------.
| yyacceptlab -- YYACCEPT comes here.  |
`-------------------------------------*/
yyacceptlab:
  yyresult = 0;
  goto yyreturn;

/*-----------------------------------.
| yyabortlab -- YYABORT comes here.  |
`-----------------------------------*/
yyabortlab:
  yyresult = 1;
  goto yyreturn;

#ifndef yyoverflow
/*-------------------------------------------------.
| yyexhaustedlab -- memory exhaustion comes here.  |
`-------------------------------------------------*/
yyexhaustedlab:
  yyerror (parm, YY_("memory exhausted"));
  yyresult = 2;
  /* Fall through.  */
#endif

yyreturn:
  if (yychar != YYEOF && yychar != YYEMPTY)
     yydestruct ("Cleanup: discarding lookahead",
		 yytoken, &yylval, parm);
  /* Do not reclaim the symbols of the rule which action triggered
     this YYABORT or YYACCEPT.  */
  YYPOPSTACK (yylen);
  YY_STACK_PRINT (yyss, yyssp);
  while (yyssp != yyss)
    {
      yydestruct ("Cleanup: popping",
		  yystos[*yyssp], yyvsp, parm);
      YYPOPSTACK (1);
    }
#ifndef yyoverflow
  if (yyss != yyssa)
    YYSTACK_FREE (yyss);
#endif
#if YYERROR_VERBOSE
  if (yymsg != yymsgbuf)
    YYSTACK_FREE (yymsg);
#endif
  /* Make sure YYID is used.  */
  return YYID (yyresult);
}



