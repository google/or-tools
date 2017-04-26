/* A Bison parser, made by GNU Bison 3.0.2.  */

/* Bison implementation for Yacc-like parsers in C

   Copyright (C) 1984, 1989-1990, 2000-2013 Free Software Foundation, Inc.

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
#define YYBISON_VERSION "3.0.2"

/* Skeleton name.  */
#define YYSKELETON_NAME "yacc.c"

/* Pure parsers.  */
#define YYPURE 1

/* Push parsers.  */
#define YYPUSH 0

/* Pull parsers.  */
#define YYPULL 1


/* Substitute the variable and function names.  */
#define yyparse         orfz_parse
#define yylex           orfz_lex
#define yyerror         orfz_error
#define yydebug         orfz_debug
#define yynerrs         orfz_nerrs


/* Copy the first part of user declarations.  */

#line 73 "ortools/flatzinc/parser.tab.cc" /* yacc.c:339  */

# ifndef YY_NULLPTR
#  if defined __cplusplus && 201103L <= __cplusplus
#   define YY_NULLPTR nullptr
#  else
#   define YY_NULLPTR 0
#  endif
# endif

/* Enabling verbose error messages.  */
#ifdef YYERROR_VERBOSE
# undef YYERROR_VERBOSE
# define YYERROR_VERBOSE 1
#else
# define YYERROR_VERBOSE 1
#endif

/* In a future release of Bison, this section will be replaced
   by #include "parser.tab.hh".  */
#ifndef YY_ORFZ_SRC_FLATZINC_PARSER_TAB_HH_INCLUDED
# define YY_ORFZ_SRC_FLATZINC_PARSER_TAB_HH_INCLUDED
/* Debug traces.  */
#ifndef YYDEBUG
# define YYDEBUG 1
#endif
#if YYDEBUG
extern int orfz_debug;
#endif
/* "%code requires" blocks.  */
#line 21 "ortools/flatzinc/parser.yy" /* yacc.c:355  */

#if !defined(OR_TOOLS_FLATZINC_FLATZINC_TAB_HH_)
#define OR_TOOLS_FLATZINC_FLATZINC_TAB_HH_
#include "ortools/base/strutil.h"
#include "ortools/base/stringpiece_utils.h"
#include "ortools/flatzinc/parser_util.h"

// Tells flex to use the LexerInfo class to communicate with the bison parser.
typedef operations_research::fz::LexerInfo YYSTYPE;

// Defines the parameter to the orfz_lex() call from the orfz_parse() method.
#define YYLEX_PARAM scanner

#endif  // OR_TOOLS_FLATZINC_FLATZINC_TAB_HH_

#line 119 "ortools/flatzinc/parser.tab.cc" /* yacc.c:355  */

/* Token type.  */
#ifndef YYTOKENTYPE
# define YYTOKENTYPE
  enum yytokentype
  {
    ARRAY = 258,
    BOOL = 259,
    CONSTRAINT = 260,
    FLOAT = 261,
    INT = 262,
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



int orfz_parse (operations_research::fz::ParserContext* context, operations_research::fz::Model* model, bool* ok, void* scanner);

#endif /* !YY_ORFZ_SRC_FLATZINC_PARSER_TAB_HH_INCLUDED  */

/* Copy the second part of user declarations.  */

#line 158 "ortools/flatzinc/parser.tab.cc" /* yacc.c:358  */
/* Unqualified %code blocks.  */
#line 38 "ortools/flatzinc/parser.yy" /* yacc.c:359  */

#include "ortools/flatzinc/parser_util.cc"

using operations_research::fz::Annotation;
using operations_research::fz::Argument;
using operations_research::fz::Constraint;
using operations_research::fz::Domain;
using operations_research::fz::IntegerVariable;
using operations_research::fz::Lookup;
using operations_research::fz::Model;
using operations_research::fz::SolutionOutputSpecs;
using operations_research::fz::ParserContext;
using operations_research::fz::VariableRefOrValue;
using operations_research::fz::VariableRefOrValueArray;

#line 176 "ortools/flatzinc/parser.tab.cc" /* yacc.c:359  */

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
#else
typedef signed char yytype_int8;
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
# elif ! defined YYSIZE_T
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
#   define YY_(Msgid) dgettext ("bison-runtime", Msgid)
#  endif
# endif
# ifndef YY_
#  define YY_(Msgid) Msgid
# endif
#endif

#ifndef YY_ATTRIBUTE
# if (defined __GNUC__                                               \
      && (2 < __GNUC__ || (__GNUC__ == 2 && 96 <= __GNUC_MINOR__)))  \
     || defined __SUNPRO_C && 0x5110 <= __SUNPRO_C
#  define YY_ATTRIBUTE(Spec) __attribute__(Spec)
# else
#  define YY_ATTRIBUTE(Spec) /* empty */
# endif
#endif

#ifndef YY_ATTRIBUTE_PURE
# define YY_ATTRIBUTE_PURE   YY_ATTRIBUTE ((__pure__))
#endif

#ifndef YY_ATTRIBUTE_UNUSED
# define YY_ATTRIBUTE_UNUSED YY_ATTRIBUTE ((__unused__))
#endif

#if !defined _Noreturn \
     && (!defined __STDC_VERSION__ || __STDC_VERSION__ < 201112)
# if defined _MSC_VER && 1200 <= _MSC_VER
#  define _Noreturn __declspec (noreturn)
# else
#  define _Noreturn YY_ATTRIBUTE ((__noreturn__))
# endif
#endif

/* Suppress unused-variable warnings by "using" E.  */
#if ! defined lint || defined __GNUC__
# define YYUSE(E) ((void) (E))
#else
# define YYUSE(E) /* empty */
#endif

#if defined __GNUC__ && 407 <= __GNUC__ * 100 + __GNUC_MINOR__
/* Suppress an incorrect diagnostic about yylval being uninitialized.  */
# define YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN \
    _Pragma ("GCC diagnostic push") \
    _Pragma ("GCC diagnostic ignored \"-Wuninitialized\"")\
    _Pragma ("GCC diagnostic ignored \"-Wmaybe-uninitialized\"")
# define YY_IGNORE_MAYBE_UNINITIALIZED_END \
    _Pragma ("GCC diagnostic pop")
#else
# define YY_INITIAL_VALUE(Value) Value
#endif
#ifndef YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
# define YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
# define YY_IGNORE_MAYBE_UNINITIALIZED_END
#endif
#ifndef YY_INITIAL_VALUE
# define YY_INITIAL_VALUE(Value) /* Nothing. */
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
#    if ! defined _ALLOCA_H && ! defined EXIT_SUCCESS
#     include <stdlib.h> /* INFRINGES ON USER NAME SPACE */
      /* Use EXIT_SUCCESS as a witness for stdlib.h.  */
#     ifndef EXIT_SUCCESS
#      define EXIT_SUCCESS 0
#     endif
#    endif
#   endif
#  endif
# endif

# ifdef YYSTACK_ALLOC
   /* Pacify GCC's 'empty if-body' warning.  */
#  define YYSTACK_FREE(Ptr) do { /* empty */; } while (0)
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
#  if (defined __cplusplus && ! defined EXIT_SUCCESS \
       && ! ((defined YYMALLOC || defined malloc) \
             && (defined YYFREE || defined free)))
#   include <stdlib.h> /* INFRINGES ON USER NAME SPACE */
#   ifndef EXIT_SUCCESS
#    define EXIT_SUCCESS 0
#   endif
#  endif
#  ifndef YYMALLOC
#   define YYMALLOC malloc
#   if ! defined malloc && ! defined EXIT_SUCCESS
void *malloc (YYSIZE_T); /* INFRINGES ON USER NAME SPACE */
#   endif
#  endif
#  ifndef YYFREE
#   define YYFREE free
#   if ! defined free && ! defined EXIT_SUCCESS
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
  yytype_int16 yyss_alloc;
  YYSTYPE yyvs_alloc;
};

/* The size of the maximum gap between one aligned stack and the next.  */
# define YYSTACK_GAP_MAXIMUM (sizeof (union yyalloc) - 1)

/* The size of an array large to enough to hold all stacks, each with
   N elements.  */
# define YYSTACK_BYTES(N) \
     ((N) * (sizeof (yytype_int16) + sizeof (YYSTYPE)) \
      + YYSTACK_GAP_MAXIMUM)

# define YYCOPY_NEEDED 1

/* Relocate STACK from its old location to the new one.  The
   local variables YYSIZE and YYSTACKSIZE give the old and new number of
   elements in the stack, and YYPTR gives the new location of the
   stack.  Advance YYPTR to a properly aligned location for the next
   stack.  */
# define YYSTACK_RELOCATE(Stack_alloc, Stack)                           \
    do                                                                  \
      {                                                                 \
        YYSIZE_T yynewbytes;                                            \
        YYCOPY (&yyptr->Stack_alloc, Stack, yysize);                    \
        Stack = &yyptr->Stack_alloc;                                    \
        yynewbytes = yystacksize * sizeof (*Stack) + YYSTACK_GAP_MAXIMUM; \
        yyptr += yynewbytes / sizeof (*yyptr);                          \
      }                                                                 \
    while (0)

#endif

#if defined YYCOPY_NEEDED && YYCOPY_NEEDED
/* Copy COUNT objects from SRC to DST.  The source and destination do
   not overlap.  */
# ifndef YYCOPY
#  if defined __GNUC__ && 1 < __GNUC__
#   define YYCOPY(Dst, Src, Count) \
      __builtin_memcpy (Dst, Src, (Count) * sizeof (*(Src)))
#  else
#   define YYCOPY(Dst, Src, Count)              \
      do                                        \
        {                                       \
          YYSIZE_T yyi;                         \
          for (yyi = 0; yyi < (Count); yyi++)   \
            (Dst)[yyi] = (Src)[yyi];            \
        }                                       \
      while (0)
#  endif
# endif
#endif /* !YYCOPY_NEEDED */

/* YYFINAL -- State number of the termination state.  */
#define YYFINAL  3
/* YYLAST -- Last index in YYTABLE.  */
#define YYLAST   255

/* YYNTOKENS -- Number of terminals.  */
#define YYNTOKENS  32
/* YYNNTS -- Number of nonterminals.  */
#define YYNNTS  30
/* YYNRULES -- Number of rules.  */
#define YYNRULES  89
/* YYNSTATES -- Number of states.  */
#define YYNSTATES  206

/* YYTRANSLATE[YYX] -- Symbol number corresponding to YYX as returned
   by yylex, with out-of-bounds checking.  */
#define YYUNDEFTOK  2
#define YYMAXUTOK   276

#define YYTRANSLATE(YYX)                                                \
  ((unsigned int) (YYX) <= YYMAXUTOK ? yytranslate[YYX] : YYUNDEFTOK)

/* YYTRANSLATE[TOKEN-NUM] -- Symbol number corresponding to TOKEN-NUM
   as returned by yylex, without out-of-bounds checking.  */
static const yytype_uint8 yytranslate[] =
{
       0,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
      23,    24,     2,     2,    25,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,    26,    22,
       2,    29,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,    27,     2,    28,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,     2,     2,     2,     2,     2,     2,     2,
       2,     2,     2,    30,     2,    31,     2,     2,     2,     2,
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
      15,    16,    17,    18,    19,    20,    21
};

#if YYDEBUG
  /* YYRLINE[YYN] -- Source line where rule number YYN was defined.  */
static const yytype_uint16 yyrline[] =
{
       0,   100,   100,   107,   111,   112,   117,   120,   121,   124,
     125,   126,   127,   130,   131,   134,   135,   142,   143,   146,
     165,   180,   190,   216,   249,   319,   320,   323,   324,   325,
     328,   332,   338,   339,   352,   370,   371,   372,   373,   380,
     381,   382,   383,   390,   391,   395,   396,   397,   400,   401,
     404,   405,   406,   411,   412,   413,   418,   419,   420,   421,
     427,   431,   437,   438,   441,   468,   469,   472,   473,   474,
     475,   476,   481,   501,   518,   543,   552,   556,   559,   560,
     563,   564,   565,   566,   576,   585,   591,   606,   614,   623
};
#endif

#if YYDEBUG || YYERROR_VERBOSE || 1
/* YYTNAME[SYMBOL-NUM] -- String name of the symbol SYMBOL-NUM.
   First, the terminals, then, starting at YYNTOKENS, nonterminals.  */
static const char *const yytname[] =
{
  "$end", "error", "$undefined", "ARRAY", "BOOL", "CONSTRAINT", "FLOAT",
  "INT", "MAXIMIZE", "MINIMIZE", "OF", "PREDICATE", "SATISFY", "SET",
  "SOLVE", "VAR", "DOTDOT", "COLONCOLON", "IVALUE", "SVALUE", "IDENTIFIER",
  "DVALUE", "';'", "'('", "')'", "','", "':'", "'['", "']'", "'='", "'{'",
  "'}'", "$accept", "model", "predicates", "predicate",
  "predicate_arguments", "predicate_argument", "predicate_array_argument",
  "predicate_ints", "variable_or_constant_declarations",
  "variable_or_constant_declaration", "optional_var_or_value",
  "optional_var_or_value_array", "var_or_value_array", "var_or_value",
  "int_domain", "set_domain", "float_domain", "domain", "integers",
  "integer", "const_literal", "const_literals", "constraints",
  "constraint", "arguments", "argument", "annotations",
  "annotation_arguments", "annotation", "solve", YY_NULLPTR
};
#endif

# ifdef YYPRINT
/* YYTOKNUM[NUM] -- (External) token number corresponding to the
   (internal) symbol number NUM (which must be that of a token).  */
static const yytype_uint16 yytoknum[] =
{
       0,   256,   257,   258,   259,   260,   261,   262,   263,   264,
     265,   266,   267,   268,   269,   270,   271,   272,   273,   274,
     275,   276,    59,    40,    41,    44,    58,    91,    93,    61,
     123,   125
};
# endif

#define YYPACT_NINF -108

#define yypact_value_is_default(Yystate) \
  (!!((Yystate) == (-108)))

#define YYTABLE_NINF -19

#define yytable_value_is_error(Yytable_value) \
  0

  /* YYPACT[STATE-NUM] -- Index in YYTABLE of the portion describing
     STATE-NUM.  */
static const yytype_int16 yypact[] =
{
    -108,    31,     7,  -108,   -15,    39,    27,    20,  -108,    62,
    -108,    66,  -108,  -108,  -108,    99,    60,   101,   131,   114,
     140,  -108,  -108,  -108,   138,   109,    40,   145,    12,   139,
     148,   146,  -108,   141,   106,  -108,  -108,   149,   150,  -108,
     151,   152,   144,    60,   153,   147,   154,   159,  -108,  -108,
     160,   114,   158,  -108,  -108,   161,   114,  -108,  -108,   162,
     107,  -108,  -108,    47,   155,  -108,    40,   163,   164,   166,
     108,  -108,   165,  -108,     0,    77,    77,    77,  -108,   102,
     167,   170,   168,  -108,   169,  -108,  -108,   171,  -108,  -108,
      22,  -108,    81,   172,  -108,   173,  -108,    64,   114,   134,
    -108,  -108,  -108,   174,  -108,    29,   102,  -108,   180,   176,
     181,  -108,   185,   137,  -108,   182,   175,  -108,    14,  -108,
     179,   183,  -108,   177,  -108,   115,  -108,   110,  -108,    77,
     187,   102,   188,   120,  -108,  -108,  -108,    56,    76,  -108,
     189,   190,  -108,   113,  -108,   184,   191,   137,  -108,  -108,
     186,  -108,  -108,   136,   192,   102,  -108,    60,   193,    60,
     195,   196,  -108,   197,  -108,  -108,   198,  -108,  -108,  -108,
    -108,   201,   194,   202,   203,   204,  -108,  -108,   209,  -108,
     210,  -108,  -108,  -108,  -108,    58,    59,    83,   205,   206,
     207,  -108,    85,    81,    90,  -108,   121,  -108,   126,  -108,
     128,  -108,    81,  -108,  -108,  -108
};

  /* YYDEFACT[STATE-NUM] -- Default reduction number in state STATE-NUM.
     Performed when YYTABLE does not specify something else to do.  Zero
     means the default is an error.  */
static const yytype_uint8 yydefact[] =
{
       5,     0,     0,     1,     0,     0,     0,    63,     4,     0,
       3,     0,    35,    43,    36,     0,     0,     0,     0,     0,
       0,    45,    46,    47,     0,     0,     0,     0,     0,     0,
       0,     0,    50,    51,     0,    49,    17,     0,     0,    77,
       0,     0,     0,     0,     0,     8,     0,     0,    39,    40,
       0,     0,     0,    37,    44,     0,     0,    38,    77,     0,
       0,    62,     2,     0,     0,     6,     0,     0,     0,     0,
       0,    77,     0,    48,     0,     0,     0,     0,    87,     0,
      16,     0,     0,    13,     0,     7,     9,     0,    41,    42,
      26,    52,     0,    67,    69,    72,    68,     0,     0,     0,
      66,    89,    88,    81,    82,    83,     0,    76,     0,     0,
       0,    10,     0,     0,    23,    53,    58,    57,     0,    19,
       0,     0,    32,    33,    75,     0,    31,     0,    77,     0,
       0,     0,     0,     0,    79,    15,    14,     0,     0,    25,
       0,     0,    56,     0,    70,     0,     0,     0,    74,    71,
      64,    65,    80,     0,     0,     0,    86,     0,     0,     0,
       0,     0,    54,     0,    55,    73,     0,    30,    84,    85,
      78,     0,     0,     0,     0,     0,    59,    34,     0,    11,
       0,    77,    77,    12,    77,     0,     0,    29,     0,     0,
       0,    24,     0,     0,     0,    21,     0,    61,     0,    28,
       0,    20,     0,    22,    27,    60
};

  /* YYPGOTO[NTERM-NUM].  */
static const yytype_int16 yypgoto[] =
{
    -108,  -108,  -108,  -108,   178,  -108,  -108,   103,  -108,  -108,
    -108,  -108,    16,  -107,    75,    78,  -108,    -7,   -50,   199,
     -66,  -108,  -108,  -108,  -108,   -72,   -56,    84,   -76,  -108
};

  /* YYDEFGOTO[NTERM-NUM].  */
static const yytype_int16 yydefgoto[] =
{
      -1,     1,     2,     6,    44,    45,    82,    83,     7,    20,
     114,   191,   125,   126,    21,    22,    23,    46,    34,    35,
     119,   198,    25,    40,    99,   100,    60,   133,   134,    41
};

  /* YYTABLE[YYPACT[STATE-NUM]] -- What to do in state STATE-NUM.  If
     positive, shift that token.  If negative, reduce the rule whose
     number is the opposite.  If YYTABLE_NINF, syntax error.  */
static const yytype_int16 yytable[] =
{
      24,    70,    74,   107,   101,   102,   139,     8,     4,    29,
     -18,   -18,   -18,   -18,   -18,    90,    48,    79,     5,    49,
     -18,   -18,   -18,    11,    12,   -18,    13,    14,   -18,    92,
      50,     3,    32,    15,    33,    16,    64,   -18,    17,    79,
     167,    18,    51,    42,    12,   142,    13,    14,   127,    10,
      19,   113,   131,    15,    80,    43,   132,   151,    17,     9,
      12,    18,    13,    14,    12,    81,    13,    14,   143,    15,
      19,   157,   150,    15,    17,    79,    79,    18,    17,   170,
      12,    18,   122,    14,   123,    26,    19,   188,   189,    15,
      19,   159,   124,    27,    17,    93,    94,    95,    96,   115,
      79,   116,   117,    32,    97,    33,    19,    98,   122,    28,
     123,   118,   190,   195,    38,    76,    77,    30,   199,    78,
     103,   104,   105,    39,    79,   185,   186,   197,   187,   106,
     158,    56,    32,    56,    33,    56,   205,    57,    56,    89,
     147,   149,   196,   148,   164,   155,    56,    31,   156,   201,
     171,   202,   173,   147,   203,   122,   204,   123,   128,   129,
     168,   155,    36,    47,    37,    52,    53,    54,    55,    58,
      59,    63,    66,    61,    62,    68,    69,    65,    71,    72,
      67,    84,    87,    86,    88,    75,   109,    80,   120,   111,
     130,   137,   108,    91,   136,   138,   110,   144,   140,   112,
     121,   145,   141,    79,   146,   152,   154,   162,   163,   166,
     200,   135,   165,   160,   179,   153,   161,     0,     0,   172,
     169,   174,   175,   181,   182,   176,   177,   178,   180,   183,
     184,     0,   192,   193,   194,     0,     0,     0,     0,     0,
       0,     0,     0,     0,    85,     0,     0,     0,     0,     0,
       0,     0,     0,     0,     0,    73
};

static const yytype_int16 yycheck[] =
{
       7,    51,    58,    79,    76,    77,   113,    22,     1,    16,
       3,     4,     5,     6,     7,    71,     4,    17,    11,     7,
      13,    14,    15,     3,     4,    18,     6,     7,    21,    29,
      18,     0,    18,    13,    20,    15,    43,    30,    18,    17,
     147,    21,    30,     3,     4,    31,     6,     7,    98,    22,
      30,    29,    23,    13,     7,    15,    27,   129,    18,    20,
       4,    21,     6,     7,     4,    18,     6,     7,   118,    13,
      30,    15,   128,    13,    18,    17,    17,    21,    18,   155,
       4,    21,    18,     7,    20,    23,    30,    29,    29,    13,
      30,    15,    28,    27,    18,    18,    19,    20,    21,    18,
      17,    20,    21,    18,    27,    20,    30,    30,    18,    10,
      20,    30,    29,    28,     5,     8,     9,    16,    28,    12,
      18,    19,    20,    14,    17,   181,   182,   193,   184,    27,
     137,    25,    18,    25,    20,    25,   202,    31,    25,    31,
      25,    31,   192,    28,    31,    25,    25,    16,    28,    28,
     157,    25,   159,    25,    28,    18,    28,    20,    24,    25,
      24,    25,    22,    18,    26,    26,    18,    21,    27,    20,
      20,    27,    25,    22,    22,    16,    16,    24,    20,    18,
      26,    26,    18,    20,    18,    23,    16,     7,    16,    20,
      16,    10,    25,    28,    18,    10,    28,    18,    16,    28,
      27,    18,    27,    17,    27,    18,    18,    18,    18,    18,
     194,   108,    28,   138,    20,   131,   138,    -1,    -1,    26,
      28,    26,    26,    20,    20,    28,    28,    26,    26,    20,
      20,    -1,    27,    27,    27,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    66,    -1,    -1,    -1,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    56
};

  /* YYSTOS[STATE-NUM] -- The (internal number of the) accessing
     symbol of state STATE-NUM.  */
static const yytype_uint8 yystos[] =
{
       0,    33,    34,     0,     1,    11,    35,    40,    22,    20,
      22,     3,     4,     6,     7,    13,    15,    18,    21,    30,
      41,    46,    47,    48,    49,    54,    23,    27,    10,    49,
      16,    16,    18,    20,    50,    51,    22,    26,     5,    14,
      55,    61,     3,    15,    36,    37,    49,    18,     4,     7,
      18,    30,    26,    18,    21,    27,    25,    31,    20,    20,
      58,    22,    22,    27,    49,    24,    25,    26,    16,    16,
      50,    20,    18,    51,    58,    23,     8,     9,    12,    17,
       7,    18,    38,    39,    26,    36,    20,    18,    18,    31,
      58,    28,    29,    18,    19,    20,    21,    27,    30,    56,
      57,    57,    57,    18,    19,    20,    27,    60,    25,    16,
      28,    20,    28,    29,    42,    18,    20,    21,    30,    52,
      16,    27,    18,    20,    28,    44,    45,    50,    24,    25,
      16,    23,    27,    59,    60,    39,    18,    10,    10,    45,
      16,    27,    31,    50,    18,    18,    27,    25,    28,    31,
      58,    57,    18,    59,    18,    25,    28,    15,    49,    15,
      46,    47,    18,    18,    31,    28,    18,    45,    24,    28,
      60,    49,    26,    49,    26,    26,    28,    28,    26,    20,
      26,    20,    20,    20,    20,    58,    58,    58,    29,    29,
      29,    43,    27,    27,    27,    28,    50,    52,    53,    28,
      44,    28,    25,    28,    28,    52
};

  /* YYR1[YYN] -- Symbol number of symbol that rule YYN derives.  */
static const yytype_uint8 yyr1[] =
{
       0,    32,    33,    34,    34,    34,    35,    36,    36,    37,
      37,    37,    37,    38,    38,    39,    39,    40,    40,    41,
      41,    41,    41,    41,    41,    42,    42,    43,    43,    43,
      44,    44,    45,    45,    45,    46,    46,    46,    46,    47,
      47,    47,    47,    48,    48,    49,    49,    49,    50,    50,
      51,    51,    51,    52,    52,    52,    52,    52,    52,    52,
      53,    53,    54,    54,    55,    56,    56,    57,    57,    57,
      57,    57,    57,    57,    57,    57,    58,    58,    59,    59,
      60,    60,    60,    60,    60,    60,    60,    61,    61,    61
};

  /* YYR2[YYN] -- Number of symbols on the right hand side of rule YYN.  */
static const yytype_uint8 yyr2[] =
{
       0,     2,     5,     3,     3,     0,     5,     3,     1,     3,
       4,     8,     9,     1,     3,     3,     1,     3,     0,     6,
      15,    14,    15,     6,    13,     2,     0,     4,     3,     0,
       3,     1,     1,     1,     4,     1,     1,     3,     3,     3,
       3,     5,     5,     1,     3,     1,     1,     1,     3,     1,
       1,     1,     4,     1,     3,     3,     2,     1,     1,     4,
       3,     1,     3,     0,     6,     3,     1,     1,     1,     1,
       3,     3,     1,     4,     3,     2,     3,     0,     3,     1,
       3,     1,     1,     1,     4,     4,     3,     3,     4,     4
};


#define yyerrok         (yyerrstatus = 0)
#define yyclearin       (yychar = YYEMPTY)
#define YYEMPTY         (-2)
#define YYEOF           0

#define YYACCEPT        goto yyacceptlab
#define YYABORT         goto yyabortlab
#define YYERROR         goto yyerrorlab


#define YYRECOVERING()  (!!yyerrstatus)

#define YYBACKUP(Token, Value)                                  \
do                                                              \
  if (yychar == YYEMPTY)                                        \
    {                                                           \
      yychar = (Token);                                         \
      yylval = (Value);                                         \
      YYPOPSTACK (yylen);                                       \
      yystate = *yyssp;                                         \
      goto yybackup;                                            \
    }                                                           \
  else                                                          \
    {                                                           \
      yyerror (context, model, ok, scanner, YY_("syntax error: cannot back up")); \
      YYERROR;                                                  \
    }                                                           \
while (0)

/* Error token number */
#define YYTERROR        1
#define YYERRCODE       256



/* Enable debugging if requested.  */
#if YYDEBUG

# ifndef YYFPRINTF
#  include <stdio.h> /* INFRINGES ON USER NAME SPACE */
#  define YYFPRINTF fprintf
# endif

# define YYDPRINTF(Args)                        \
do {                                            \
  if (yydebug)                                  \
    YYFPRINTF Args;                             \
} while (0)

/* This macro is provided for backward compatibility. */
#ifndef YY_LOCATION_PRINT
# define YY_LOCATION_PRINT(File, Loc) ((void) 0)
#endif


# define YY_SYMBOL_PRINT(Title, Type, Value, Location)                    \
do {                                                                      \
  if (yydebug)                                                            \
    {                                                                     \
      YYFPRINTF (stderr, "%s ", Title);                                   \
      yy_symbol_print (stderr,                                            \
                  Type, Value, context, model, ok, scanner); \
      YYFPRINTF (stderr, "\n");                                           \
    }                                                                     \
} while (0)


/*----------------------------------------.
| Print this symbol's value on YYOUTPUT.  |
`----------------------------------------*/

static void
yy_symbol_value_print (FILE *yyoutput, int yytype, YYSTYPE const * const yyvaluep, operations_research::fz::ParserContext* context, operations_research::fz::Model* model, bool* ok, void* scanner)
{
  FILE *yyo = yyoutput;
  YYUSE (yyo);
  YYUSE (context);
  YYUSE (model);
  YYUSE (ok);
  YYUSE (scanner);
  if (!yyvaluep)
    return;
# ifdef YYPRINT
  if (yytype < YYNTOKENS)
    YYPRINT (yyoutput, yytoknum[yytype], *yyvaluep);
# endif
  YYUSE (yytype);
}


/*--------------------------------.
| Print this symbol on YYOUTPUT.  |
`--------------------------------*/

static void
yy_symbol_print (FILE *yyoutput, int yytype, YYSTYPE const * const yyvaluep, operations_research::fz::ParserContext* context, operations_research::fz::Model* model, bool* ok, void* scanner)
{
  YYFPRINTF (yyoutput, "%s %s (",
             yytype < YYNTOKENS ? "token" : "nterm", yytname[yytype]);

  yy_symbol_value_print (yyoutput, yytype, yyvaluep, context, model, ok, scanner);
  YYFPRINTF (yyoutput, ")");
}

/*------------------------------------------------------------------.
| yy_stack_print -- Print the state stack from its BOTTOM up to its |
| TOP (included).                                                   |
`------------------------------------------------------------------*/

static void
yy_stack_print (yytype_int16 *yybottom, yytype_int16 *yytop)
{
  YYFPRINTF (stderr, "Stack now");
  for (; yybottom <= yytop; yybottom++)
    {
      int yybot = *yybottom;
      YYFPRINTF (stderr, " %d", yybot);
    }
  YYFPRINTF (stderr, "\n");
}

# define YY_STACK_PRINT(Bottom, Top)                            \
do {                                                            \
  if (yydebug)                                                  \
    yy_stack_print ((Bottom), (Top));                           \
} while (0)


/*------------------------------------------------.
| Report that the YYRULE is going to be reduced.  |
`------------------------------------------------*/

static void
yy_reduce_print (yytype_int16 *yyssp, YYSTYPE *yyvsp, int yyrule, operations_research::fz::ParserContext* context, operations_research::fz::Model* model, bool* ok, void* scanner)
{
  unsigned long int yylno = yyrline[yyrule];
  int yynrhs = yyr2[yyrule];
  int yyi;
  YYFPRINTF (stderr, "Reducing stack by rule %d (line %lu):\n",
             yyrule - 1, yylno);
  /* The symbols being reduced.  */
  for (yyi = 0; yyi < yynrhs; yyi++)
    {
      YYFPRINTF (stderr, "   $%d = ", yyi + 1);
      yy_symbol_print (stderr,
                       yystos[yyssp[yyi + 1 - yynrhs]],
                       &(yyvsp[(yyi + 1) - (yynrhs)])
                                              , context, model, ok, scanner);
      YYFPRINTF (stderr, "\n");
    }
}

# define YY_REDUCE_PRINT(Rule)          \
do {                                    \
  if (yydebug)                          \
    yy_reduce_print (yyssp, yyvsp, Rule, context, model, ok, scanner); \
} while (0)

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
#ifndef YYINITDEPTH
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
static YYSIZE_T
yystrlen (const char *yystr)
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
static char *
yystpcpy (char *yydest, const char *yysrc)
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

/* Copy into *YYMSG, which is of size *YYMSG_ALLOC, an error message
   about the unexpected token YYTOKEN for the state stack whose top is
   YYSSP.

   Return 0 if *YYMSG was successfully written.  Return 1 if *YYMSG is
   not large enough to hold the message.  In that case, also set
   *YYMSG_ALLOC to the required number of bytes.  Return 2 if the
   required number of bytes is too large to store.  */
static int
yysyntax_error (YYSIZE_T *yymsg_alloc, char **yymsg,
                yytype_int16 *yyssp, int yytoken)
{
  YYSIZE_T yysize0 = yytnamerr (YY_NULLPTR, yytname[yytoken]);
  YYSIZE_T yysize = yysize0;
  enum { YYERROR_VERBOSE_ARGS_MAXIMUM = 5 };
  /* Internationalized format string. */
  const char *yyformat = YY_NULLPTR;
  /* Arguments of yyformat. */
  char const *yyarg[YYERROR_VERBOSE_ARGS_MAXIMUM];
  /* Number of reported tokens (one for the "unexpected", one per
     "expected"). */
  int yycount = 0;

  /* There are many possibilities here to consider:
     - If this state is a consistent state with a default action, then
       the only way this function was invoked is if the default action
       is an error action.  In that case, don't check for expected
       tokens because there are none.
     - The only way there can be no lookahead present (in yychar) is if
       this state is a consistent state with a default action.  Thus,
       detecting the absence of a lookahead is sufficient to determine
       that there is no unexpected or expected token to report.  In that
       case, just report a simple "syntax error".
     - Don't assume there isn't a lookahead just because this state is a
       consistent state with a default action.  There might have been a
       previous inconsistent state, consistent state with a non-default
       action, or user semantic action that manipulated yychar.
     - Of course, the expected token list depends on states to have
       correct lookahead information, and it depends on the parser not
       to perform extra reductions after fetching a lookahead from the
       scanner and before detecting a syntax error.  Thus, state merging
       (from LALR or IELR) and default reductions corrupt the expected
       token list.  However, the list is correct for canonical LR with
       one exception: it will still contain any token that will not be
       accepted due to an error action in a later state.
  */
  if (yytoken != YYEMPTY)
    {
      int yyn = yypact[*yyssp];
      yyarg[yycount++] = yytname[yytoken];
      if (!yypact_value_is_default (yyn))
        {
          /* Start YYX at -YYN if negative to avoid negative indexes in
             YYCHECK.  In other words, skip the first -YYN actions for
             this state because they are default actions.  */
          int yyxbegin = yyn < 0 ? -yyn : 0;
          /* Stay within bounds of both yycheck and yytname.  */
          int yychecklim = YYLAST - yyn + 1;
          int yyxend = yychecklim < YYNTOKENS ? yychecklim : YYNTOKENS;
          int yyx;

          for (yyx = yyxbegin; yyx < yyxend; ++yyx)
            if (yycheck[yyx + yyn] == yyx && yyx != YYTERROR
                && !yytable_value_is_error (yytable[yyx + yyn]))
              {
                if (yycount == YYERROR_VERBOSE_ARGS_MAXIMUM)
                  {
                    yycount = 1;
                    yysize = yysize0;
                    break;
                  }
                yyarg[yycount++] = yytname[yyx];
                {
                  YYSIZE_T yysize1 = yysize + yytnamerr (YY_NULLPTR, yytname[yyx]);
                  if (! (yysize <= yysize1
                         && yysize1 <= YYSTACK_ALLOC_MAXIMUM))
                    return 2;
                  yysize = yysize1;
                }
              }
        }
    }

  switch (yycount)
    {
# define YYCASE_(N, S)                      \
      case N:                               \
        yyformat = S;                       \
      break
      YYCASE_(0, YY_("syntax error"));
      YYCASE_(1, YY_("syntax error, unexpected %s"));
      YYCASE_(2, YY_("syntax error, unexpected %s, expecting %s"));
      YYCASE_(3, YY_("syntax error, unexpected %s, expecting %s or %s"));
      YYCASE_(4, YY_("syntax error, unexpected %s, expecting %s or %s or %s"));
      YYCASE_(5, YY_("syntax error, unexpected %s, expecting %s or %s or %s or %s"));
# undef YYCASE_
    }

  {
    YYSIZE_T yysize1 = yysize + yystrlen (yyformat);
    if (! (yysize <= yysize1 && yysize1 <= YYSTACK_ALLOC_MAXIMUM))
      return 2;
    yysize = yysize1;
  }

  if (*yymsg_alloc < yysize)
    {
      *yymsg_alloc = 2 * yysize;
      if (! (yysize <= *yymsg_alloc
             && *yymsg_alloc <= YYSTACK_ALLOC_MAXIMUM))
        *yymsg_alloc = YYSTACK_ALLOC_MAXIMUM;
      return 1;
    }

  /* Avoid sprintf, as that infringes on the user's name space.
     Don't have undefined behavior even if the translation
     produced a string with the wrong number of "%s"s.  */
  {
    char *yyp = *yymsg;
    int yyi = 0;
    while ((*yyp = *yyformat) != '\0')
      if (*yyp == '%' && yyformat[1] == 's' && yyi < yycount)
        {
          yyp += yytnamerr (yyp, yyarg[yyi++]);
          yyformat += 2;
        }
      else
        {
          yyp++;
          yyformat++;
        }
  }
  return 0;
}
#endif /* YYERROR_VERBOSE */

/*-----------------------------------------------.
| Release the memory associated to this symbol.  |
`-----------------------------------------------*/

static void
yydestruct (const char *yymsg, int yytype, YYSTYPE *yyvaluep, operations_research::fz::ParserContext* context, operations_research::fz::Model* model, bool* ok, void* scanner)
{
  YYUSE (yyvaluep);
  YYUSE (context);
  YYUSE (model);
  YYUSE (ok);
  YYUSE (scanner);
  if (!yymsg)
    yymsg = "Deleting";
  YY_SYMBOL_PRINT (yymsg, yytype, yyvaluep, yylocationp);

  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  YYUSE (yytype);
  YY_IGNORE_MAYBE_UNINITIALIZED_END
}




/*----------.
| yyparse.  |
`----------*/

int
yyparse (operations_research::fz::ParserContext* context, operations_research::fz::Model* model, bool* ok, void* scanner)
{
/* The lookahead symbol.  */
int yychar;


/* The semantic value of the lookahead symbol.  */
/* Default value used for initialization, for pacifying older GCCs
   or non-GCC compilers.  */
YY_INITIAL_VALUE (static YYSTYPE yyval_default;)
YYSTYPE yylval YY_INITIAL_VALUE (= yyval_default);

    /* Number of syntax errors so far.  */
    int yynerrs;

    int yystate;
    /* Number of tokens to shift before error messages enabled.  */
    int yyerrstatus;

    /* The stacks and their tools:
       'yyss': related to states.
       'yyvs': related to semantic values.

       Refer to the stacks through separate pointers, to allow yyoverflow
       to reallocate them elsewhere.  */

    /* The state stack.  */
    yytype_int16 yyssa[YYINITDEPTH];
    yytype_int16 *yyss;
    yytype_int16 *yyssp;

    /* The semantic value stack.  */
    YYSTYPE yyvsa[YYINITDEPTH];
    YYSTYPE *yyvs;
    YYSTYPE *yyvsp;

    YYSIZE_T yystacksize;

  int yyn;
  int yyresult;
  /* Lookahead token as an internal (translated) token number.  */
  int yytoken = 0;
  /* The variables used to return semantic value and location from the
     action routines.  */
  YYSTYPE yyval;

#if YYERROR_VERBOSE
  /* Buffer for error messages, and its allocated size.  */
  char yymsgbuf[128];
  char *yymsg = yymsgbuf;
  YYSIZE_T yymsg_alloc = sizeof yymsgbuf;
#endif

#define YYPOPSTACK(N)   (yyvsp -= (N), yyssp -= (N))

  /* The number of symbols on the RHS of the reduced rule.
     Keep to zero when no symbol should be popped.  */
  int yylen = 0;

  yyssp = yyss = yyssa;
  yyvsp = yyvs = yyvsa;
  yystacksize = YYINITDEPTH;

  YYDPRINTF ((stderr, "Starting parse\n"));

  yystate = 0;
  yyerrstatus = 0;
  yynerrs = 0;
  yychar = YYEMPTY; /* Cause a token to be read.  */
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
        YYSTACK_RELOCATE (yyss_alloc, yyss);
        YYSTACK_RELOCATE (yyvs_alloc, yyvs);
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

  if (yystate == YYFINAL)
    YYACCEPT;

  goto yybackup;

/*-----------.
| yybackup.  |
`-----------*/
yybackup:

  /* Do appropriate processing given the current state.  Read a
     lookahead token if we need one and don't already have one.  */

  /* First try to decide what to do without reference to lookahead token.  */
  yyn = yypact[yystate];
  if (yypact_value_is_default (yyn))
    goto yydefault;

  /* Not known => get a lookahead token if don't already have one.  */

  /* YYCHAR is either YYEMPTY or YYEOF or a valid lookahead symbol.  */
  if (yychar == YYEMPTY)
    {
      YYDPRINTF ((stderr, "Reading a token: "));
      yychar = yylex (&yylval, scanner);
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
      if (yytable_value_is_error (yyn))
        goto yyerrlab;
      yyn = -yyn;
      goto yyreduce;
    }

  /* Count tokens shifted since error; after three, turn off error
     status.  */
  if (yyerrstatus)
    yyerrstatus--;

  /* Shift the lookahead token.  */
  YY_SYMBOL_PRINT ("Shifting", yytoken, &yylval, &yylloc);

  /* Discard the shifted token.  */
  yychar = YYEMPTY;

  yystate = yyn;
  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  *++yyvsp = yylval;
  YY_IGNORE_MAYBE_UNINITIALIZED_END

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
     '$$ = $1'.

     Otherwise, the following line sets YYVAL to garbage.
     This behavior is undocumented and Bison
     users should not rely upon it.  Assigning to YYVAL
     unconditionally makes the parser a bit smaller, and it avoids a
     GCC warning that YYVAL may be used uninitialized.  */
  yyval = yyvsp[1-yylen];


  YY_REDUCE_PRINT (yyn);
  switch (yyn)
    {
        case 4:
#line 111 "ortools/flatzinc/parser.yy" /* yacc.c:1646  */
    { yyerrok; }
#line 1407 "ortools/flatzinc/parser.tab.cc" /* yacc.c:1646  */
    break;

  case 19:
#line 146 "ortools/flatzinc/parser.yy" /* yacc.c:1646  */
    {
  // Declaration of a (named) constant: we simply register it in the
  // parser's context, and don't store it in the model.
  const Domain& domain = (yyvsp[-5].domain);
  const std::string& identifier = (yyvsp[-3].string_value);
  const Domain& assignment = (yyvsp[0].domain);
  std::vector<Annotation>* const annotations = (yyvsp[-2].annotations);


  if (!assignment.HasOneValue()) {
    // TODO(lperron): Check that the assignment is included in the domain.
    context->domain_map[identifier] = assignment;
  } else {
    const int64 value = assignment.values.front();
    CHECK(domain.Contains(value));
    context->integer_map[identifier] = value;
  }
  delete annotations;
}
#line 1431 "ortools/flatzinc/parser.tab.cc" /* yacc.c:1646  */
    break;

  case 20:
#line 166 "ortools/flatzinc/parser.yy" /* yacc.c:1646  */
    {
  std::vector<Annotation>* const annotations = (yyvsp[-4].annotations);
  // Declaration of a (named) constant array. See rule right above.
  CHECK_EQ((yyvsp[-12].integer_value), 1) << "Only [1..n] array are supported here.";
  const int64 num_constants = (yyvsp[-10].integer_value);
  const std::string& identifier = (yyvsp[-5].string_value);
  const std::vector<int64>* const assignments = (yyvsp[-1].integers);
  CHECK(assignments != nullptr);
  CHECK_EQ(num_constants, assignments->size());
  // TODO(lperron): CHECK all values within domain.
  context->integer_array_map[identifier] = *assignments;
  delete assignments;
  delete annotations;
}
#line 1450 "ortools/flatzinc/parser.tab.cc" /* yacc.c:1646  */
    break;

  case 21:
#line 181 "ortools/flatzinc/parser.yy" /* yacc.c:1646  */
    {
  std::vector<Annotation>* const annotations = (yyvsp[-3].annotations);
  // Declaration of a (named) constant array. See rule right above.
  CHECK_EQ((yyvsp[-11].integer_value), 1) << "Only [1..n] array are supported here.";
  const int64 num_constants = (yyvsp[-9].integer_value);
  CHECK_EQ(num_constants, 0) << "Empty arrays should have a size of 0";
  const std::string& identifier = (yyvsp[-4].string_value);
  context->integer_array_map[identifier] = std::vector<int64>();
  delete annotations;
}
#line 1465 "ortools/flatzinc/parser.tab.cc" /* yacc.c:1646  */
    break;

  case 22:
#line 191 "ortools/flatzinc/parser.yy" /* yacc.c:1646  */
    {
  // Declaration of a (named) constant array: See rule above.
  CHECK_EQ((yyvsp[-12].integer_value), 1) << "Only [1..n] array are supported here.";
  const int64 num_constants = (yyvsp[-10].integer_value);
  const Domain& domain = (yyvsp[-7].domain);
  const std::string& identifier = (yyvsp[-5].string_value);
  const std::vector<Domain>* const assignments = (yyvsp[-1].domains);
  const std::vector<Annotation>* const annotations = (yyvsp[-4].annotations);
  CHECK(assignments != nullptr);
  CHECK_EQ(num_constants, assignments->size());

  if (!AllDomainsHaveOneValue(*assignments)) {
    context->domain_array_map[identifier] = *assignments;
    // TODO(lperron): check that all assignments are included in the domain.
  } else {
    std::vector<int64> values(num_constants);
    for (int i = 0; i < num_constants; ++i) {
      values[i] = (*assignments)[i].values.front();
      CHECK(domain.Contains(values[i]));
    }
    context->integer_array_map[identifier] = values;
  }
  delete assignments;
  delete annotations;
}
#line 1495 "ortools/flatzinc/parser.tab.cc" /* yacc.c:1646  */
    break;

  case 23:
#line 216 "ortools/flatzinc/parser.yy" /* yacc.c:1646  */
    {
  // Declaration of a variable. If it's unassigned or assigned to a
  // constant, we'll create a new var stored in the model. If it's
  // assigned to another variable x then we simply adjust that
  // existing variable x according to the current (re-)declaration.
  const Domain& domain = (yyvsp[-4].domain);
  const std::string& identifier = (yyvsp[-2].string_value);
  std::vector<Annotation>* const annotations = (yyvsp[-1].annotations);
  const VariableRefOrValue& assignment = (yyvsp[0].var_or_value);
  const bool introduced = ContainsId(annotations, "var_is_introduced") ||
      strings::StartsWith(identifier, "X_INTRODUCED");
  IntegerVariable* var = nullptr;
  if (!assignment.defined) {
    var = model->AddVariable(identifier, domain, introduced);
  } else if (assignment.variable == nullptr) {  // just an integer constant.
    CHECK(domain.Contains(assignment.value));
    var = model->AddVariable(
        identifier, Domain::IntegerValue(assignment.value), introduced);
  } else {  // a variable.
    var = assignment.variable;
    var->Merge(identifier, domain, nullptr, introduced);
  }

  // We also register the variable in the parser's context, and add some
  // output to the model if needed.
  context->variable_map[identifier] = var;
  if (ContainsId(annotations, "output_var")) {
    model->AddOutput(
        SolutionOutputSpecs::SingleVariable(identifier, var,
                                           domain.display_as_boolean));
  }
  delete annotations;
}
#line 1533 "ortools/flatzinc/parser.tab.cc" /* yacc.c:1646  */
    break;

  case 24:
#line 250 "ortools/flatzinc/parser.yy" /* yacc.c:1646  */
    {
  // Declaration of a "variable array": these is exactly like N simple
  // variable declarations, where the identifier for declaration #i is
  // IDENTIFIER[i] (1-based index).
  CHECK_EQ((yyvsp[-10].integer_value), 1);
  const int64 num_vars = (yyvsp[-8].integer_value);
  const Domain& domain = (yyvsp[-4].domain);
  const std::string& identifier = (yyvsp[-2].string_value);
  std::vector<Annotation>* const annotations = (yyvsp[-1].annotations);
  VariableRefOrValueArray* const assignments = (yyvsp[0].var_or_value_array);
  CHECK(assignments == nullptr || assignments->variables.size() == num_vars);
  CHECK(assignments == nullptr || assignments->values.size() == num_vars);
  const bool introduced = ContainsId(annotations, "var_is_introduced") ||
      strings::StartsWith(identifier, "X_INTRODUCED");

  std::vector<IntegerVariable*> vars(num_vars, nullptr);

  for (int i = 0; i < num_vars; ++i) {
    const std::string var_name = StringPrintf("%s[%d]", identifier.c_str(), i + 1);
    if (assignments == nullptr) {
      vars[i] = model->AddVariable(var_name, domain, introduced);
    } else if (assignments->variables[i] == nullptr) {
      // Assigned to an integer constant.
      const int64 value = assignments->values[i];
      CHECK(domain.Contains(value));
      vars[i] =
          model->AddVariable(var_name, Domain::IntegerValue(value), introduced);
    } else {
      IntegerVariable* const var = assignments->variables[i];
      CHECK(var != nullptr);
      vars[i] = var;
      vars[i]->Merge(var_name, domain, nullptr, introduced);
    }
  }
  delete assignments;

  // Register the variable array on the context.
  context->variable_array_map[identifier] = vars;

  // We parse the annotations to build an output object if
  // needed. It's a bit more convoluted than the simple variable
  // output.
  if (annotations != nullptr) {
    for (int i = 0; i < annotations->size(); ++i) {
      const Annotation& ann = (*annotations)[i];
      if (ann.IsFunctionCallWithIdentifier("output_array")) {
        // We have found an output annotation.
        CHECK_EQ(1, ann.annotations.size());
        CHECK_EQ(Annotation::ANNOTATION_LIST, ann.annotations.back().type);
        const Annotation& list = ann.annotations.back();
        // Let's build the vector of bounds.
        std::vector<SolutionOutputSpecs::Bounds> bounds;
        for (int a = 0; a < list.annotations.size(); ++a) {
          const Annotation& bound = list.annotations[a];
          CHECK_EQ(Annotation::INTERVAL, bound.type);
          bounds.emplace_back(
              SolutionOutputSpecs::Bounds(bound.interval_min, bound.interval_max));
        }
        // We add the output information.
        model->AddOutput(
            SolutionOutputSpecs::MultiDimensionalArray(identifier, bounds, vars,
      domain.display_as_boolean));
      }
    }
    delete annotations;
  }
}
#line 1605 "ortools/flatzinc/parser.tab.cc" /* yacc.c:1646  */
    break;

  case 25:
#line 319 "ortools/flatzinc/parser.yy" /* yacc.c:1646  */
    { (yyval.var_or_value) = (yyvsp[0].var_or_value); }
#line 1611 "ortools/flatzinc/parser.tab.cc" /* yacc.c:1646  */
    break;

  case 26:
#line 320 "ortools/flatzinc/parser.yy" /* yacc.c:1646  */
    { (yyval.var_or_value) = VariableRefOrValue::Undefined(); }
#line 1617 "ortools/flatzinc/parser.tab.cc" /* yacc.c:1646  */
    break;

  case 27:
#line 323 "ortools/flatzinc/parser.yy" /* yacc.c:1646  */
    { (yyval.var_or_value_array) = (yyvsp[-1].var_or_value_array); }
#line 1623 "ortools/flatzinc/parser.tab.cc" /* yacc.c:1646  */
    break;

  case 28:
#line 324 "ortools/flatzinc/parser.yy" /* yacc.c:1646  */
    { (yyval.var_or_value_array) = nullptr; }
#line 1629 "ortools/flatzinc/parser.tab.cc" /* yacc.c:1646  */
    break;

  case 29:
#line 325 "ortools/flatzinc/parser.yy" /* yacc.c:1646  */
    { (yyval.var_or_value_array) = nullptr; }
#line 1635 "ortools/flatzinc/parser.tab.cc" /* yacc.c:1646  */
    break;

  case 30:
#line 328 "ortools/flatzinc/parser.yy" /* yacc.c:1646  */
    {
  (yyval.var_or_value_array) = (yyvsp[-2].var_or_value_array);
  (yyval.var_or_value_array)->PushBack((yyvsp[0].var_or_value));
}
#line 1644 "ortools/flatzinc/parser.tab.cc" /* yacc.c:1646  */
    break;

  case 31:
#line 332 "ortools/flatzinc/parser.yy" /* yacc.c:1646  */
    {
  (yyval.var_or_value_array) = new VariableRefOrValueArray();
  (yyval.var_or_value_array)->PushBack((yyvsp[0].var_or_value));
}
#line 1653 "ortools/flatzinc/parser.tab.cc" /* yacc.c:1646  */
    break;

  case 32:
#line 338 "ortools/flatzinc/parser.yy" /* yacc.c:1646  */
    { (yyval.var_or_value) = VariableRefOrValue::Value((yyvsp[0].integer_value)); }
#line 1659 "ortools/flatzinc/parser.tab.cc" /* yacc.c:1646  */
    break;

  case 33:
#line 339 "ortools/flatzinc/parser.yy" /* yacc.c:1646  */
    {
  // A reference to an existing integer constant or variable.
  const std::string& id = (yyvsp[0].string_value);
  if (ContainsKey(context->integer_map, id)) {
    (yyval.var_or_value) = VariableRefOrValue::Value(FindOrDie(context->integer_map, id));
  } else if (ContainsKey(context->variable_map, id)) {
    (yyval.var_or_value) = VariableRefOrValue::VariableRef(FindOrDie(context->variable_map, id));
  } else {
    LOG(ERROR) << "Unknown symbol " << id;
    (yyval.var_or_value) = VariableRefOrValue::Undefined();
    *ok = false;
  }
}
#line 1677 "ortools/flatzinc/parser.tab.cc" /* yacc.c:1646  */
    break;

  case 34:
#line 352 "ortools/flatzinc/parser.yy" /* yacc.c:1646  */
    {
  // A given element of an existing constant array or variable array.
  const std::string& id = (yyvsp[-3].string_value);
  const int64 value = (yyvsp[-1].integer_value);
  if (ContainsKey(context->integer_array_map, id)) {
    (yyval.var_or_value) = VariableRefOrValue::Value(
        Lookup(FindOrDie(context->integer_array_map, id), value));
  } else if (ContainsKey(context->variable_array_map, id)) {
    (yyval.var_or_value) = VariableRefOrValue::VariableRef(
        Lookup(FindOrDie(context->variable_array_map, id), value));
  } else {
    LOG(ERROR) << "Unknown symbol " << id;
    (yyval.var_or_value) = VariableRefOrValue::Undefined();
    *ok = false;
  }
}
#line 1698 "ortools/flatzinc/parser.tab.cc" /* yacc.c:1646  */
    break;

  case 35:
#line 370 "ortools/flatzinc/parser.yy" /* yacc.c:1646  */
    { (yyval.domain) = Domain::Boolean(); }
#line 1704 "ortools/flatzinc/parser.tab.cc" /* yacc.c:1646  */
    break;

  case 36:
#line 371 "ortools/flatzinc/parser.yy" /* yacc.c:1646  */
    { (yyval.domain) = Domain::AllInt64(); }
#line 1710 "ortools/flatzinc/parser.tab.cc" /* yacc.c:1646  */
    break;

  case 37:
#line 372 "ortools/flatzinc/parser.yy" /* yacc.c:1646  */
    { (yyval.domain) = Domain::Interval((yyvsp[-2].integer_value), (yyvsp[0].integer_value)); }
#line 1716 "ortools/flatzinc/parser.tab.cc" /* yacc.c:1646  */
    break;

  case 38:
#line 373 "ortools/flatzinc/parser.yy" /* yacc.c:1646  */
    {
  CHECK((yyvsp[-1].integers) != nullptr);
  (yyval.domain) = Domain::IntegerList(std::move(*(yyvsp[-1].integers)));
  delete (yyvsp[-1].integers);
}
#line 1726 "ortools/flatzinc/parser.tab.cc" /* yacc.c:1646  */
    break;

  case 39:
#line 380 "ortools/flatzinc/parser.yy" /* yacc.c:1646  */
    { (yyval.domain) = Domain::Boolean(); }
#line 1732 "ortools/flatzinc/parser.tab.cc" /* yacc.c:1646  */
    break;

  case 40:
#line 381 "ortools/flatzinc/parser.yy" /* yacc.c:1646  */
    { (yyval.domain) = Domain::AllInt64(); }
#line 1738 "ortools/flatzinc/parser.tab.cc" /* yacc.c:1646  */
    break;

  case 41:
#line 382 "ortools/flatzinc/parser.yy" /* yacc.c:1646  */
    { (yyval.domain) = Domain::Interval((yyvsp[-2].integer_value), (yyvsp[0].integer_value)); }
#line 1744 "ortools/flatzinc/parser.tab.cc" /* yacc.c:1646  */
    break;

  case 42:
#line 383 "ortools/flatzinc/parser.yy" /* yacc.c:1646  */
    {
  CHECK((yyvsp[-1].integers) != nullptr);
  (yyval.domain) = Domain::IntegerList(std::move(*(yyvsp[-1].integers)));
  delete (yyvsp[-1].integers);
}
#line 1754 "ortools/flatzinc/parser.tab.cc" /* yacc.c:1646  */
    break;

  case 43:
#line 390 "ortools/flatzinc/parser.yy" /* yacc.c:1646  */
    { (yyval.domain) = Domain::AllInt64(); }
#line 1760 "ortools/flatzinc/parser.tab.cc" /* yacc.c:1646  */
    break;

  case 44:
#line 391 "ortools/flatzinc/parser.yy" /* yacc.c:1646  */
    { (yyval.domain) = Domain::AllInt64(); }
#line 1766 "ortools/flatzinc/parser.tab.cc" /* yacc.c:1646  */
    break;

  case 45:
#line 395 "ortools/flatzinc/parser.yy" /* yacc.c:1646  */
    { (yyval.domain) = (yyvsp[0].domain); }
#line 1772 "ortools/flatzinc/parser.tab.cc" /* yacc.c:1646  */
    break;

  case 46:
#line 396 "ortools/flatzinc/parser.yy" /* yacc.c:1646  */
    { (yyval.domain) = (yyvsp[0].domain); }
#line 1778 "ortools/flatzinc/parser.tab.cc" /* yacc.c:1646  */
    break;

  case 47:
#line 397 "ortools/flatzinc/parser.yy" /* yacc.c:1646  */
    { (yyval.domain) = (yyvsp[0].domain); }
#line 1784 "ortools/flatzinc/parser.tab.cc" /* yacc.c:1646  */
    break;

  case 48:
#line 400 "ortools/flatzinc/parser.yy" /* yacc.c:1646  */
    { (yyval.integers) = (yyvsp[-2].integers); (yyval.integers)->emplace_back((yyvsp[0].integer_value)); }
#line 1790 "ortools/flatzinc/parser.tab.cc" /* yacc.c:1646  */
    break;

  case 49:
#line 401 "ortools/flatzinc/parser.yy" /* yacc.c:1646  */
    { (yyval.integers) = new std::vector<int64>(); (yyval.integers)->emplace_back((yyvsp[0].integer_value)); }
#line 1796 "ortools/flatzinc/parser.tab.cc" /* yacc.c:1646  */
    break;

  case 50:
#line 404 "ortools/flatzinc/parser.yy" /* yacc.c:1646  */
    { (yyval.integer_value) = (yyvsp[0].integer_value); }
#line 1802 "ortools/flatzinc/parser.tab.cc" /* yacc.c:1646  */
    break;

  case 51:
#line 405 "ortools/flatzinc/parser.yy" /* yacc.c:1646  */
    { (yyval.integer_value) = FindOrDie(context->integer_map, (yyvsp[0].string_value)); }
#line 1808 "ortools/flatzinc/parser.tab.cc" /* yacc.c:1646  */
    break;

  case 52:
#line 406 "ortools/flatzinc/parser.yy" /* yacc.c:1646  */
    {
  (yyval.integer_value) = Lookup(FindOrDie(context->integer_array_map, (yyvsp[-3].string_value)), (yyvsp[-1].integer_value));
}
#line 1816 "ortools/flatzinc/parser.tab.cc" /* yacc.c:1646  */
    break;

  case 53:
#line 411 "ortools/flatzinc/parser.yy" /* yacc.c:1646  */
    { (yyval.domain) = Domain::IntegerValue((yyvsp[0].integer_value)); }
#line 1822 "ortools/flatzinc/parser.tab.cc" /* yacc.c:1646  */
    break;

  case 54:
#line 412 "ortools/flatzinc/parser.yy" /* yacc.c:1646  */
    { (yyval.domain) = Domain::Interval((yyvsp[-2].integer_value), (yyvsp[0].integer_value)); }
#line 1828 "ortools/flatzinc/parser.tab.cc" /* yacc.c:1646  */
    break;

  case 55:
#line 413 "ortools/flatzinc/parser.yy" /* yacc.c:1646  */
    {
  CHECK((yyvsp[-1].integers) != nullptr);
  (yyval.domain) = Domain::IntegerList(std::move(*(yyvsp[-1].integers)));
  delete (yyvsp[-1].integers);
}
#line 1838 "ortools/flatzinc/parser.tab.cc" /* yacc.c:1646  */
    break;

  case 56:
#line 418 "ortools/flatzinc/parser.yy" /* yacc.c:1646  */
    { (yyval.domain) = Domain::EmptyDomain(); }
#line 1844 "ortools/flatzinc/parser.tab.cc" /* yacc.c:1646  */
    break;

  case 57:
#line 419 "ortools/flatzinc/parser.yy" /* yacc.c:1646  */
    { (yyval.domain) = Domain::AllInt64(); }
#line 1850 "ortools/flatzinc/parser.tab.cc" /* yacc.c:1646  */
    break;

  case 58:
#line 420 "ortools/flatzinc/parser.yy" /* yacc.c:1646  */
    { (yyval.domain) = Domain::IntegerValue(FindOrDie(context->integer_map, (yyvsp[0].string_value))); }
#line 1856 "ortools/flatzinc/parser.tab.cc" /* yacc.c:1646  */
    break;

  case 59:
#line 421 "ortools/flatzinc/parser.yy" /* yacc.c:1646  */
    {
  (yyval.domain) = Domain::IntegerValue(
      Lookup(FindOrDie(context->integer_array_map, (yyvsp[-3].string_value)), (yyvsp[-1].integer_value)));
}
#line 1865 "ortools/flatzinc/parser.tab.cc" /* yacc.c:1646  */
    break;

  case 60:
#line 427 "ortools/flatzinc/parser.yy" /* yacc.c:1646  */
    {
  (yyval.domains) = (yyvsp[-2].domains);
  (yyval.domains)->emplace_back((yyvsp[0].domain));
}
#line 1874 "ortools/flatzinc/parser.tab.cc" /* yacc.c:1646  */
    break;

  case 61:
#line 431 "ortools/flatzinc/parser.yy" /* yacc.c:1646  */
    { (yyval.domains) = new std::vector<Domain>(); (yyval.domains)->emplace_back((yyvsp[0].domain)); }
#line 1880 "ortools/flatzinc/parser.tab.cc" /* yacc.c:1646  */
    break;

  case 64:
#line 441 "ortools/flatzinc/parser.yy" /* yacc.c:1646  */
    {
  const std::string& identifier = (yyvsp[-4].string_value);
  CHECK((yyvsp[-2].args) != nullptr) << "Missing argument in constraint";
  const std::vector<Argument>& arguments = *(yyvsp[-2].args);
  std::vector<Annotation>* const annotations = (yyvsp[0].annotations);

  // Does the constraint has a defines_var annotation?
  IntegerVariable* defines_var = nullptr;
  if (annotations != nullptr) {
    for (int i = 0; i < annotations->size(); ++i) {
      const Annotation& ann = (*annotations)[i];
      if (ann.IsFunctionCallWithIdentifier("defines_var")) {
        CHECK_EQ(1, ann.annotations.size());
        CHECK_EQ(Annotation::INT_VAR_REF, ann.annotations.back().type);
        defines_var = ann.annotations.back().variables[0];
        break;
      }
    }
  }

  model->AddConstraint(identifier, arguments,
                       ContainsId(annotations, "domain"), defines_var);
  delete annotations;
  delete (yyvsp[-2].args);
}
#line 1910 "ortools/flatzinc/parser.tab.cc" /* yacc.c:1646  */
    break;

  case 65:
#line 468 "ortools/flatzinc/parser.yy" /* yacc.c:1646  */
    { (yyval.args) = (yyvsp[-2].args); (yyval.args)->emplace_back((yyvsp[0].arg)); }
#line 1916 "ortools/flatzinc/parser.tab.cc" /* yacc.c:1646  */
    break;

  case 66:
#line 469 "ortools/flatzinc/parser.yy" /* yacc.c:1646  */
    { (yyval.args) = new std::vector<Argument>(); (yyval.args)->emplace_back((yyvsp[0].arg)); }
#line 1922 "ortools/flatzinc/parser.tab.cc" /* yacc.c:1646  */
    break;

  case 67:
#line 472 "ortools/flatzinc/parser.yy" /* yacc.c:1646  */
    { (yyval.arg) = Argument::IntegerValue((yyvsp[0].integer_value)); }
#line 1928 "ortools/flatzinc/parser.tab.cc" /* yacc.c:1646  */
    break;

  case 68:
#line 473 "ortools/flatzinc/parser.yy" /* yacc.c:1646  */
    { (yyval.arg) = Argument::VoidArgument(); }
#line 1934 "ortools/flatzinc/parser.tab.cc" /* yacc.c:1646  */
    break;

  case 69:
#line 474 "ortools/flatzinc/parser.yy" /* yacc.c:1646  */
    { (yyval.arg) = Argument::VoidArgument(); }
#line 1940 "ortools/flatzinc/parser.tab.cc" /* yacc.c:1646  */
    break;

  case 70:
#line 475 "ortools/flatzinc/parser.yy" /* yacc.c:1646  */
    { (yyval.arg) = Argument::Interval((yyvsp[-2].integer_value), (yyvsp[0].integer_value)); }
#line 1946 "ortools/flatzinc/parser.tab.cc" /* yacc.c:1646  */
    break;

  case 71:
#line 476 "ortools/flatzinc/parser.yy" /* yacc.c:1646  */
    {
  CHECK((yyvsp[-1].integers) != nullptr);
  (yyval.arg) = Argument::IntegerList(std::move(*(yyvsp[-1].integers)));
  delete (yyvsp[-1].integers);
}
#line 1956 "ortools/flatzinc/parser.tab.cc" /* yacc.c:1646  */
    break;

  case 72:
#line 481 "ortools/flatzinc/parser.yy" /* yacc.c:1646  */
    {
  const std::string& id = (yyvsp[0].string_value);
  if (ContainsKey(context->integer_map, id)) {
    (yyval.arg) = Argument::IntegerValue(FindOrDie(context->integer_map, id));
  } else if (ContainsKey(context->integer_array_map, id)) {
    (yyval.arg) = Argument::IntegerList(FindOrDie(context->integer_array_map, id));
  } else if (ContainsKey(context->variable_map, id)) {
    (yyval.arg) = Argument::IntVarRef(FindOrDie(context->variable_map, id));
  } else if (ContainsKey(context->variable_array_map, id)) {
    (yyval.arg) = Argument::IntVarRefArray(FindOrDie(context->variable_array_map, id));
  } else if (ContainsKey(context->domain_map, id)) {
    const Domain& d = FindOrDie(context->domain_map, id);
    (yyval.arg) = Argument::FromDomain(d);
  } else {
    CHECK(ContainsKey(context->domain_array_map, id)) << "Unknown identifier: "
                                                      << id;
    const std::vector<Domain>& d = FindOrDie(context->domain_array_map, id);
    (yyval.arg) = Argument::DomainList(d);
  }
}
#line 1981 "ortools/flatzinc/parser.tab.cc" /* yacc.c:1646  */
    break;

  case 73:
#line 501 "ortools/flatzinc/parser.yy" /* yacc.c:1646  */
    {
  const std::string& id = (yyvsp[-3].string_value);
  const int64 index = (yyvsp[-1].integer_value);
  if (ContainsKey(context->integer_array_map, id)) {
    (yyval.arg) = Argument::IntegerValue(
        Lookup(FindOrDie(context->integer_array_map, id), index));
  } else if (ContainsKey(context->variable_array_map, id)) {
    (yyval.arg) = Argument::IntVarRef(
        Lookup(FindOrDie(context->variable_array_map, id), index));
  } else {
    CHECK(ContainsKey(context->domain_array_map, id))
        << "Unknown identifier: " << id;
    const Domain& d =
        Lookup(FindOrDie(context->domain_array_map, id), index);
    (yyval.arg) = Argument::FromDomain(d);
  }
}
#line 2003 "ortools/flatzinc/parser.tab.cc" /* yacc.c:1646  */
    break;

  case 74:
#line 518 "ortools/flatzinc/parser.yy" /* yacc.c:1646  */
    {
  VariableRefOrValueArray* const arguments = (yyvsp[-1].var_or_value_array);
  CHECK(arguments != nullptr);
  bool has_variables = false;
  for (int i = 0; i < arguments->Size(); ++i) {
    if (arguments->variables[i] != nullptr) {
      has_variables = true;
      break;
    }
  }
  if (has_variables) {
    (yyval.arg) = Argument::IntVarRefArray(std::vector<IntegerVariable*>());
    (yyval.arg).variables.reserve(arguments->Size());
    for (int i = 0; i < arguments->Size(); ++i) {
      if (arguments->variables[i] != nullptr) {
         (yyval.arg).variables.emplace_back(arguments->variables[i]);
      } else {
         (yyval.arg).variables.emplace_back(model->AddConstant(arguments->values[i]));
      }
    }
  } else {
    (yyval.arg) = Argument::IntegerList(arguments->values);
  }
  delete arguments;
}
#line 2033 "ortools/flatzinc/parser.tab.cc" /* yacc.c:1646  */
    break;

  case 75:
#line 543 "ortools/flatzinc/parser.yy" /* yacc.c:1646  */
    {
  (yyval.arg) = Argument::VoidArgument();
}
#line 2041 "ortools/flatzinc/parser.tab.cc" /* yacc.c:1646  */
    break;

  case 76:
#line 552 "ortools/flatzinc/parser.yy" /* yacc.c:1646  */
    {
    (yyval.annotations) = (yyvsp[-2].annotations) != nullptr ? (yyvsp[-2].annotations) : new std::vector<Annotation>();
    (yyval.annotations)->emplace_back((yyvsp[0].annotation));
  }
#line 2050 "ortools/flatzinc/parser.tab.cc" /* yacc.c:1646  */
    break;

  case 77:
#line 556 "ortools/flatzinc/parser.yy" /* yacc.c:1646  */
    { (yyval.annotations) = nullptr; }
#line 2056 "ortools/flatzinc/parser.tab.cc" /* yacc.c:1646  */
    break;

  case 78:
#line 559 "ortools/flatzinc/parser.yy" /* yacc.c:1646  */
    { (yyval.annotations) = (yyvsp[-2].annotations); (yyval.annotations)->emplace_back((yyvsp[0].annotation)); }
#line 2062 "ortools/flatzinc/parser.tab.cc" /* yacc.c:1646  */
    break;

  case 79:
#line 560 "ortools/flatzinc/parser.yy" /* yacc.c:1646  */
    { (yyval.annotations) = new std::vector<Annotation>(); (yyval.annotations)->emplace_back((yyvsp[0].annotation)); }
#line 2068 "ortools/flatzinc/parser.tab.cc" /* yacc.c:1646  */
    break;

  case 80:
#line 563 "ortools/flatzinc/parser.yy" /* yacc.c:1646  */
    { (yyval.annotation) = Annotation::Interval((yyvsp[-2].integer_value), (yyvsp[0].integer_value)); }
#line 2074 "ortools/flatzinc/parser.tab.cc" /* yacc.c:1646  */
    break;

  case 81:
#line 564 "ortools/flatzinc/parser.yy" /* yacc.c:1646  */
    { (yyval.annotation) = Annotation::IntegerValue((yyvsp[0].integer_value)); }
#line 2080 "ortools/flatzinc/parser.tab.cc" /* yacc.c:1646  */
    break;

  case 82:
#line 565 "ortools/flatzinc/parser.yy" /* yacc.c:1646  */
    { (yyval.annotation) = Annotation::String((yyvsp[0].string_value)); }
#line 2086 "ortools/flatzinc/parser.tab.cc" /* yacc.c:1646  */
    break;

  case 83:
#line 566 "ortools/flatzinc/parser.yy" /* yacc.c:1646  */
    {
  const std::string& id = (yyvsp[0].string_value);
  if (ContainsKey(context->variable_map, id)) {
    (yyval.annotation) = Annotation::Variable(FindOrDie(context->variable_map, id));
  } else if (ContainsKey(context->variable_array_map, id)) {
    (yyval.annotation) = Annotation::VariableList(FindOrDie(context->variable_array_map, id));
  } else {
    (yyval.annotation) = Annotation::Identifier(id);
  }
}
#line 2101 "ortools/flatzinc/parser.tab.cc" /* yacc.c:1646  */
    break;

  case 84:
#line 576 "ortools/flatzinc/parser.yy" /* yacc.c:1646  */
    {
  std::vector<Annotation>* const annotations = (yyvsp[-1].annotations);
  if (annotations != nullptr) {
    (yyval.annotation) = Annotation::FunctionCallWithArguments((yyvsp[-3].string_value), std::move(*annotations));
    delete annotations;
  } else {
    (yyval.annotation) = Annotation::FunctionCall((yyvsp[-3].string_value));
  }
}
#line 2115 "ortools/flatzinc/parser.tab.cc" /* yacc.c:1646  */
    break;

  case 85:
#line 585 "ortools/flatzinc/parser.yy" /* yacc.c:1646  */
    {
  CHECK(ContainsKey(context->variable_array_map, (yyvsp[-3].string_value)))
      << "Unknown identifier: " << (yyvsp[-3].string_value);
  (yyval.annotation) = Annotation::Variable(
      Lookup(FindOrDie(context->variable_array_map, (yyvsp[-3].string_value)), (yyvsp[-1].integer_value)));
}
#line 2126 "ortools/flatzinc/parser.tab.cc" /* yacc.c:1646  */
    break;

  case 86:
#line 591 "ortools/flatzinc/parser.yy" /* yacc.c:1646  */
    {
  std::vector<Annotation>* const annotations = (yyvsp[-1].annotations);
  if (annotations != nullptr) {
    (yyval.annotation) = Annotation::AnnotationList(std::move(*annotations));
    delete annotations;
  } else {
    (yyval.annotation) = Annotation::Empty();
  }
}
#line 2140 "ortools/flatzinc/parser.tab.cc" /* yacc.c:1646  */
    break;

  case 87:
#line 606 "ortools/flatzinc/parser.yy" /* yacc.c:1646  */
    {
  if ((yyvsp[-1].annotations) != nullptr) {
    model->Satisfy(std::move(*(yyvsp[-1].annotations)));
    delete (yyvsp[-1].annotations);
  } else {
    model->Satisfy(std::vector<Annotation>());
  }
}
#line 2153 "ortools/flatzinc/parser.tab.cc" /* yacc.c:1646  */
    break;

  case 88:
#line 614 "ortools/flatzinc/parser.yy" /* yacc.c:1646  */
    {
  CHECK_EQ(Argument::INT_VAR_REF, (yyvsp[0].arg).type);
  if ((yyvsp[-2].annotations) != nullptr) {
    model->Minimize((yyvsp[0].arg).Var(), std::move(*(yyvsp[-2].annotations)));
    delete (yyvsp[-2].annotations);
  } else {
    model->Minimize((yyvsp[0].arg).Var(), std::vector<Annotation>());
  }
}
#line 2167 "ortools/flatzinc/parser.tab.cc" /* yacc.c:1646  */
    break;

  case 89:
#line 623 "ortools/flatzinc/parser.yy" /* yacc.c:1646  */
    {
  CHECK_EQ(Argument::INT_VAR_REF, (yyvsp[0].arg).type);
  if ((yyvsp[-2].annotations) != nullptr) {
    model->Maximize((yyvsp[0].arg).Var(), std::move(*(yyvsp[-2].annotations)));
    delete (yyvsp[-2].annotations);
  } else {
    model->Maximize((yyvsp[0].arg).Var(), std::vector<Annotation>());
  }
}
#line 2181 "ortools/flatzinc/parser.tab.cc" /* yacc.c:1646  */
    break;


#line 2185 "ortools/flatzinc/parser.tab.cc" /* yacc.c:1646  */
      default: break;
    }
  /* User semantic actions sometimes alter yychar, and that requires
     that yytoken be updated with the new translation.  We take the
     approach of translating immediately before every use of yytoken.
     One alternative is translating here after every semantic action,
     but that translation would be missed if the semantic action invokes
     YYABORT, YYACCEPT, or YYERROR immediately after altering yychar or
     if it invokes YYBACKUP.  In the case of YYABORT or YYACCEPT, an
     incorrect destructor might then be invoked immediately.  In the
     case of YYERROR or YYBACKUP, subsequent parser actions might lead
     to an incorrect destructor call or verbose syntax error message
     before the lookahead is translated.  */
  YY_SYMBOL_PRINT ("-> $$ =", yyr1[yyn], &yyval, &yyloc);

  YYPOPSTACK (yylen);
  yylen = 0;
  YY_STACK_PRINT (yyss, yyssp);

  *++yyvsp = yyval;

  /* Now 'shift' the result of the reduction.  Determine what state
     that goes to, based on the state we popped back to and the rule
     number reduced by.  */

  yyn = yyr1[yyn];

  yystate = yypgoto[yyn - YYNTOKENS] + *yyssp;
  if (0 <= yystate && yystate <= YYLAST && yycheck[yystate] == *yyssp)
    yystate = yytable[yystate];
  else
    yystate = yydefgoto[yyn - YYNTOKENS];

  goto yynewstate;


/*--------------------------------------.
| yyerrlab -- here on detecting error.  |
`--------------------------------------*/
yyerrlab:
  /* Make sure we have latest lookahead translation.  See comments at
     user semantic actions for why this is necessary.  */
  yytoken = yychar == YYEMPTY ? YYEMPTY : YYTRANSLATE (yychar);

  /* If not already recovering from an error, report this error.  */
  if (!yyerrstatus)
    {
      ++yynerrs;
#if ! YYERROR_VERBOSE
      yyerror (context, model, ok, scanner, YY_("syntax error"));
#else
# define YYSYNTAX_ERROR yysyntax_error (&yymsg_alloc, &yymsg, \
                                        yyssp, yytoken)
      {
        char const *yymsgp = YY_("syntax error");
        int yysyntax_error_status;
        yysyntax_error_status = YYSYNTAX_ERROR;
        if (yysyntax_error_status == 0)
          yymsgp = yymsg;
        else if (yysyntax_error_status == 1)
          {
            if (yymsg != yymsgbuf)
              YYSTACK_FREE (yymsg);
            yymsg = (char *) YYSTACK_ALLOC (yymsg_alloc);
            if (!yymsg)
              {
                yymsg = yymsgbuf;
                yymsg_alloc = sizeof yymsgbuf;
                yysyntax_error_status = 2;
              }
            else
              {
                yysyntax_error_status = YYSYNTAX_ERROR;
                yymsgp = yymsg;
              }
          }
        yyerror (context, model, ok, scanner, yymsgp);
        if (yysyntax_error_status == 2)
          goto yyexhaustedlab;
      }
# undef YYSYNTAX_ERROR
#endif
    }



  if (yyerrstatus == 3)
    {
      /* If just tried and failed to reuse lookahead token after an
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
                      yytoken, &yylval, context, model, ok, scanner);
          yychar = YYEMPTY;
        }
    }

  /* Else will try to reuse lookahead token after shifting the error
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

  /* Do not reclaim the symbols of the rule whose action triggered
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
  yyerrstatus = 3;      /* Each real token shifted decrements this.  */

  for (;;)
    {
      yyn = yypact[yystate];
      if (!yypact_value_is_default (yyn))
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
                  yystos[yystate], yyvsp, context, model, ok, scanner);
      YYPOPSTACK (1);
      yystate = *yyssp;
      YY_STACK_PRINT (yyss, yyssp);
    }

  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  *++yyvsp = yylval;
  YY_IGNORE_MAYBE_UNINITIALIZED_END


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

#if !defined yyoverflow || YYERROR_VERBOSE
/*-------------------------------------------------.
| yyexhaustedlab -- memory exhaustion comes here.  |
`-------------------------------------------------*/
yyexhaustedlab:
  yyerror (context, model, ok, scanner, YY_("memory exhausted"));
  yyresult = 2;
  /* Fall through.  */
#endif

yyreturn:
  if (yychar != YYEMPTY)
    {
      /* Make sure we have latest lookahead translation.  See comments at
         user semantic actions for why this is necessary.  */
      yytoken = YYTRANSLATE (yychar);
      yydestruct ("Cleanup: discarding lookahead",
                  yytoken, &yylval, context, model, ok, scanner);
    }
  /* Do not reclaim the symbols of the rule whose action triggered
     this YYABORT or YYACCEPT.  */
  YYPOPSTACK (yylen);
  YY_STACK_PRINT (yyss, yyssp);
  while (yyssp != yyss)
    {
      yydestruct ("Cleanup: popping",
                  yystos[*yyssp], yyvsp, context, model, ok, scanner);
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
  return yyresult;
}
#line 633 "ortools/flatzinc/parser.yy" /* yacc.c:1906  */
