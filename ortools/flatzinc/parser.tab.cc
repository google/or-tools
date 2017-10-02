/* A Bison parser, made by GNU Bison 3.0.4.  */

/* Bison implementation for Yacc-like parsers in C

   Copyright (C) 1984, 1989-1990, 2000-2015 Free Software Foundation, Inc.

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
#define YYBISON_VERSION "3.0.4"

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

#line 73 "./ortools/flatzinc/parser.tab.cc" /* yacc.c:339  */

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
#ifndef YY_ORFZ_ORTOOLS_FLATZINC_PARSER_TAB_HH_INCLUDED
# define YY_ORFZ_ORTOOLS_FLATZINC_PARSER_TAB_HH_INCLUDED
/* Debug traces.  */
#ifndef YYDEBUG
# define YYDEBUG 1
#endif
#if YYDEBUG
extern int orfz_debug;
#endif
/* "%code requires" blocks.  */
#line 21 "./ortools/flatzinc/parser.yy" /* yacc.c:355  */

#if !defined(OR_TOOLS_FLATZINC_FLATZINC_TAB_HH_)
#define OR_TOOLS_FLATZINC_FLATZINC_TAB_HH_
#include "ortools/base/strutil.h"
#include "ortools/flatzinc/parser_util.h"

// Tells flex to use the LexerInfo class to communicate with the bison parser.
typedef operations_research::fz::LexerInfo YYSTYPE;

// Defines the parameter to the orfz_lex() call from the orfz_parse() method.
#define YYLEX_PARAM scanner

#endif  // OR_TOOLS_FLATZINC_FLATZINC_TAB_HH_

#line 118 "./ortools/flatzinc/parser.tab.cc" /* yacc.c:355  */

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

#endif /* !YY_ORFZ_ORTOOLS_FLATZINC_PARSER_TAB_HH_INCLUDED  */

/* Copy the second part of user declarations.  */

#line 157 "./ortools/flatzinc/parser.tab.cc" /* yacc.c:358  */
/* Unqualified %code blocks.  */
#line 37 "./ortools/flatzinc/parser.yy" /* yacc.c:359  */

#include "ortools/base/stringpiece_utils.h"
#include "ortools/flatzinc/parser_util.cc"

using operations_research::fz::Annotation;
using operations_research::fz::Argument;
using operations_research::fz::Constraint;
using operations_research::fz::ConvertAsIntegerOrDie;
using operations_research::fz::Domain;
using operations_research::fz::IntegerVariable;
using operations_research::fz::Lookup;
using operations_research::fz::Model;
using operations_research::fz::SolutionOutputSpecs;
using operations_research::fz::ParserContext;
using operations_research::fz::VariableRefOrValue;
using operations_research::fz::VariableRefOrValueArray;

#line 177 "./ortools/flatzinc/parser.tab.cc" /* yacc.c:359  */

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
#define YYLAST   271

/* YYNTOKENS -- Number of terminals.  */
#define YYNTOKENS  32
/* YYNNTS -- Number of nonterminals.  */
#define YYNNTS  32
/* YYNRULES -- Number of rules.  */
#define YYNRULES  96
/* YYNSTATES -- Number of states.  */
#define YYNSTATES  223

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
       0,   103,   103,   110,   114,   115,   120,   123,   124,   127,
     128,   129,   130,   133,   134,   137,   138,   145,   146,   149,
     168,   183,   194,   209,   220,   246,   279,   349,   350,   353,
     354,   355,   358,   362,   368,   369,   382,   400,   401,   402,
     403,   410,   411,   412,   413,   420,   421,   428,   429,   430,
     433,   434,   437,   438,   439,   444,   445,   448,   449,   450,
     455,   456,   457,   462,   463,   467,   468,   474,   478,   484,
     485,   488,   515,   516,   519,   520,   521,   522,   523,   528,
     559,   576,   601,   610,   614,   617,   618,   621,   622,   623,
     624,   634,   643,   649,   664,   672,   681
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
  "integer", "floats", "float", "const_literal", "const_literals",
  "constraints", "constraint", "arguments", "argument", "annotations",
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

#define YYPACT_NINF -182

#define yypact_value_is_default(Yystate) \
  (!!((Yystate) == (-182)))

#define YYTABLE_NINF -19

#define yytable_value_is_error(Yytable_value) \
  0

  /* YYPACT[STATE-NUM] -- Index in YYTABLE of the portion describing
     STATE-NUM.  */
static const yytype_int16 yypact[] =
{
    -182,    49,     7,  -182,   -15,    67,   114,    20,  -182,    95,
    -182,    99,  -182,  -182,  -182,   136,    76,   122,   141,    11,
     154,  -182,  -182,  -182,   143,   130,    40,   157,    12,   151,
     160,   158,  -182,   155,   118,  -182,  -182,   161,   163,  -182,
     162,   164,   165,    76,   156,   166,   159,   171,  -182,  -182,
     172,    11,   169,  -182,  -182,   175,    11,  -182,  -182,   167,
     125,  -182,  -182,    27,   168,  -182,    40,   176,   177,   179,
     120,  -182,   170,  -182,    22,    80,    80,    80,  -182,   121,
     174,   184,   173,  -182,   182,  -182,  -182,   178,  -182,  -182,
      59,  -182,    75,   187,  -182,   180,  -182,    93,    11,   131,
    -182,  -182,  -182,   188,  -182,    96,   121,  -182,   198,   190,
     199,  -182,   200,   150,  -182,   195,   185,  -182,    34,  -182,
     196,   197,  -182,   186,  -182,    31,  -182,   128,  -182,    80,
     201,   121,   202,    84,  -182,  -182,  -182,    56,    60,  -182,
     203,   204,  -182,   129,  -182,   189,   205,   150,  -182,  -182,
     207,  -182,  -182,   147,   206,   121,  -182,    76,   192,    76,
     209,   210,   211,  -182,   212,  -182,  -182,   213,  -182,  -182,
    -182,  -182,   216,   208,   217,   218,   219,   224,  -182,  -182,
     225,  -182,   226,  -182,  -182,  -182,  -182,  -182,    74,    85,
      87,    91,   220,   221,   222,   223,  -182,    97,    75,    64,
     104,  -182,   133,  -182,   137,   227,  -182,  -182,   138,  -182,
    -182,   139,  -182,    75,  -182,   214,   153,  -182,  -182,  -182,
     228,  -182,  -182
};

  /* YYDEFACT[STATE-NUM] -- Default reduction number in state STATE-NUM.
     Performed when YYTABLE does not specify something else to do.  Zero
     means the default is an error.  */
static const yytype_uint8 yydefact[] =
{
       5,     0,     0,     1,     0,     0,     0,    70,     4,     0,
       3,     0,    37,    45,    38,     0,     0,     0,     0,     0,
       0,    47,    48,    49,     0,     0,     0,     0,     0,     0,
       0,     0,    52,    53,     0,    51,    17,     0,     0,    84,
       0,     0,     0,     0,     0,     8,     0,     0,    41,    42,
       0,     0,     0,    39,    46,     0,     0,    40,    84,     0,
       0,    69,     2,     0,     0,     6,     0,     0,     0,     0,
       0,    84,     0,    50,     0,     0,     0,     0,    94,     0,
      16,     0,     0,    13,     0,     7,     9,     0,    43,    44,
      28,    54,     0,    74,    76,    79,    75,     0,     0,     0,
      73,    96,    95,    88,    89,    90,     0,    83,     0,     0,
       0,    10,     0,     0,    25,    60,    65,    64,     0,    19,
       0,     0,    34,    35,    82,     0,    33,     0,    84,     0,
       0,     0,     0,     0,    86,    15,    14,     0,     0,    27,
       0,     0,    63,     0,    77,     0,     0,     0,    81,    78,
      71,    72,    87,     0,     0,     0,    93,     0,     0,     0,
       0,     0,     0,    61,     0,    62,    80,     0,    32,    91,
      92,    85,     0,     0,     0,     0,     0,     0,    66,    36,
       0,    11,     0,    84,    84,    84,    12,    84,     0,     0,
       0,    31,     0,     0,     0,     0,    26,     0,     0,     0,
       0,    21,     0,    68,     0,    58,    57,    23,     0,    56,
      30,     0,    20,     0,    24,     0,     0,    22,    29,    67,
       0,    55,    59
};

  /* YYPGOTO[NTERM-NUM].  */
static const yytype_int16 yypgoto[] =
{
    -182,  -182,  -182,  -182,   191,  -182,  -182,   108,  -182,  -182,
    -182,  -182,    25,  -107,    88,    89,    92,    -7,   -50,   215,
    -182,    13,  -181,  -182,  -182,  -182,  -182,   -72,   -56,   100,
     -76,  -182
};

  /* YYDEFGOTO[NTERM-NUM].  */
static const yytype_int16 yydefgoto[] =
{
      -1,     1,     2,     6,    44,    45,    82,    83,     7,    20,
     114,   196,   125,   126,    21,    22,    23,    46,    34,    35,
     208,   209,   119,   204,    25,    40,    99,   100,    60,   133,
     134,    41
};

  /* YYTABLE[YYPACT[STATE-NUM]] -- What to do in state STATE-NUM.  If
     positive, shift that token.  If negative, reduce the rule whose
     number is the opposite.  If YYTABLE_NINF, syntax error.  */
static const yytype_int16 yytable[] =
{
      24,    70,    74,   107,   101,   102,   139,     8,     4,    29,
     -18,   -18,   -18,   -18,   -18,    90,    48,   203,     5,    49,
     -18,   -18,   -18,    11,    12,   -18,    13,    14,   -18,    32,
      50,    33,   219,    15,    80,    16,    64,   -18,    17,    79,
     168,    18,    51,    42,    12,    81,    13,    14,   127,     3,
      19,    92,    32,    15,    33,    43,   147,   151,    17,   148,
      12,    18,    13,    14,    12,   142,    13,    14,   143,    15,
      19,   157,   150,    15,    17,   159,    79,    18,    17,   171,
      12,    18,    13,    14,   205,   206,    19,     9,   113,    15,
      19,    79,   207,   115,    17,   116,   117,    18,    93,    94,
      95,    96,    79,   192,    79,   118,    19,    97,    79,   155,
      98,   122,   156,   123,   193,    32,   194,    33,    26,   131,
     195,   124,   122,   132,   123,   201,    27,   188,   189,   190,
     158,   191,   210,    76,    77,    38,    10,    78,    30,   103,
     104,   105,    79,    56,    39,    56,    28,   202,   106,    57,
     172,    89,   174,    56,    56,   128,   129,    31,    56,   149,
     165,   212,   213,   216,   147,   214,   217,   218,   122,    37,
     123,   169,   155,   205,   206,    47,    36,    52,    53,    54,
      65,    58,    55,    59,    61,    67,    62,    68,    69,    71,
      75,    66,    63,    72,    84,    87,    86,    88,    91,   108,
     109,   110,   111,   120,   130,    80,   112,   121,   136,   137,
     138,   140,   141,   146,   144,   145,   135,   166,   173,   152,
     154,   163,   164,   167,    79,   211,   160,   161,   181,   221,
     162,   153,   220,     0,   170,   175,   176,   177,   183,   184,
     178,   179,   180,   182,   185,   186,   187,   197,   198,   199,
     200,     0,     0,     0,   215,     0,   222,    85,     0,     0,
       0,     0,     0,     0,     0,     0,     0,     0,     0,     0,
       0,    73
};

static const yytype_int16 yycheck[] =
{
       7,    51,    58,    79,    76,    77,   113,    22,     1,    16,
       3,     4,     5,     6,     7,    71,     4,   198,    11,     7,
      13,    14,    15,     3,     4,    18,     6,     7,    21,    18,
      18,    20,   213,    13,     7,    15,    43,    30,    18,    17,
     147,    21,    30,     3,     4,    18,     6,     7,    98,     0,
      30,    29,    18,    13,    20,    15,    25,   129,    18,    28,
       4,    21,     6,     7,     4,    31,     6,     7,   118,    13,
      30,    15,   128,    13,    18,    15,    17,    21,    18,   155,
       4,    21,     6,     7,    20,    21,    30,    20,    29,    13,
      30,    17,    28,    18,    18,    20,    21,    21,    18,    19,
      20,    21,    17,    29,    17,    30,    30,    27,    17,    25,
      30,    18,    28,    20,    29,    18,    29,    20,    23,    23,
      29,    28,    18,    27,    20,    28,    27,   183,   184,   185,
     137,   187,    28,     8,     9,     5,    22,    12,    16,    18,
      19,    20,    17,    25,    14,    25,    10,   197,    27,    31,
     157,    31,   159,    25,    25,    24,    25,    16,    25,    31,
      31,    28,    25,    25,    25,    28,    28,    28,    18,    26,
      20,    24,    25,    20,    21,    18,    22,    26,    18,    21,
      24,    20,    27,    20,    22,    26,    22,    16,    16,    20,
      23,    25,    27,    18,    26,    18,    20,    18,    28,    25,
      16,    28,    20,    16,    16,     7,    28,    27,    18,    10,
      10,    16,    27,    27,    18,    18,   108,    28,    26,    18,
      18,    18,    18,    18,    17,   200,   138,   138,    20,   216,
     138,   131,    18,    -1,    28,    26,    26,    26,    20,    20,
      28,    28,    26,    26,    20,    20,    20,    27,    27,    27,
      27,    -1,    -1,    -1,    27,    -1,    28,    66,    -1,    -1,
      -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,    -1,
      -1,    56
};

  /* YYSTOS[STATE-NUM] -- The (internal number of the) accessing
     symbol of state STATE-NUM.  */
static const yytype_uint8 yystos[] =
{
       0,    33,    34,     0,     1,    11,    35,    40,    22,    20,
      22,     3,     4,     6,     7,    13,    15,    18,    21,    30,
      41,    46,    47,    48,    49,    56,    23,    27,    10,    49,
      16,    16,    18,    20,    50,    51,    22,    26,     5,    14,
      57,    63,     3,    15,    36,    37,    49,    18,     4,     7,
      18,    30,    26,    18,    21,    27,    25,    31,    20,    20,
      60,    22,    22,    27,    49,    24,    25,    26,    16,    16,
      50,    20,    18,    51,    60,    23,     8,     9,    12,    17,
       7,    18,    38,    39,    26,    36,    20,    18,    18,    31,
      60,    28,    29,    18,    19,    20,    21,    27,    30,    58,
      59,    59,    59,    18,    19,    20,    27,    62,    25,    16,
      28,    20,    28,    29,    42,    18,    20,    21,    30,    54,
      16,    27,    18,    20,    28,    44,    45,    50,    24,    25,
      16,    23,    27,    61,    62,    39,    18,    10,    10,    45,
      16,    27,    31,    50,    18,    18,    27,    25,    28,    31,
      60,    59,    18,    61,    18,    25,    28,    15,    49,    15,
      46,    47,    48,    18,    18,    31,    28,    18,    45,    24,
      28,    62,    49,    26,    49,    26,    26,    26,    28,    28,
      26,    20,    26,    20,    20,    20,    20,    20,    60,    60,
      60,    60,    29,    29,    29,    29,    43,    27,    27,    27,
      27,    28,    50,    54,    55,    20,    21,    28,    52,    53,
      28,    44,    28,    25,    28,    27,    25,    28,    28,    54,
      18,    53,    28
};

  /* YYR1[YYN] -- Symbol number of symbol that rule YYN derives.  */
static const yytype_uint8 yyr1[] =
{
       0,    32,    33,    34,    34,    34,    35,    36,    36,    37,
      37,    37,    37,    38,    38,    39,    39,    40,    40,    41,
      41,    41,    41,    41,    41,    41,    41,    42,    42,    43,
      43,    43,    44,    44,    45,    45,    45,    46,    46,    46,
      46,    47,    47,    47,    47,    48,    48,    49,    49,    49,
      50,    50,    51,    51,    51,    52,    52,    53,    53,    53,
      54,    54,    54,    54,    54,    54,    54,    55,    55,    56,
      56,    57,    58,    58,    59,    59,    59,    59,    59,    59,
      59,    59,    59,    60,    60,    61,    61,    62,    62,    62,
      62,    62,    62,    62,    63,    63,    63
};

  /* YYR2[YYN] -- Number of symbols on the right hand side of rule YYN.  */
static const yytype_uint8 yyr2[] =
{
       0,     2,     5,     3,     3,     0,     5,     3,     1,     3,
       4,     8,     9,     1,     3,     3,     1,     3,     0,     6,
      15,    14,    15,    14,    15,     6,    13,     2,     0,     4,
       3,     0,     3,     1,     1,     1,     4,     1,     1,     3,
       3,     3,     3,     5,     5,     1,     3,     1,     1,     1,
       3,     1,     1,     1,     4,     3,     1,     1,     1,     4,
       1,     3,     3,     2,     1,     1,     4,     3,     1,     3,
       0,     6,     3,     1,     1,     1,     1,     3,     3,     1,
       4,     3,     2,     3,     0,     3,     1,     3,     1,     1,
       1,     4,     4,     3,     3,     4,     4
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
#line 114 "./ortools/flatzinc/parser.yy" /* yacc.c:1646  */
    { yyerrok; }
#line 1423 "./ortools/flatzinc/parser.tab.cc" /* yacc.c:1646  */
    break;

  case 19:
#line 149 "./ortools/flatzinc/parser.yy" /* yacc.c:1646  */
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
#line 1447 "./ortools/flatzinc/parser.tab.cc" /* yacc.c:1646  */
    break;

  case 20:
#line 169 "./ortools/flatzinc/parser.yy" /* yacc.c:1646  */
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
#line 1466 "./ortools/flatzinc/parser.tab.cc" /* yacc.c:1646  */
    break;

  case 21:
#line 184 "./ortools/flatzinc/parser.yy" /* yacc.c:1646  */
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
#line 1481 "./ortools/flatzinc/parser.tab.cc" /* yacc.c:1646  */
    break;

  case 22:
#line 195 "./ortools/flatzinc/parser.yy" /* yacc.c:1646  */
    {
  std::vector<Annotation>* const annotations = (yyvsp[-4].annotations);
  // Declaration of a (named) constant array. See rule right above.
  CHECK_EQ((yyvsp[-12].integer_value), 1) << "Only [1..n] array are supported here.";
  const int64 num_constants = (yyvsp[-10].integer_value);
  const std::string& identifier = (yyvsp[-5].string_value);
  const std::vector<double>* const assignments = (yyvsp[-1].doubles);
  CHECK(assignments != nullptr);
  CHECK_EQ(num_constants, assignments->size());
  // TODO(lperron): CHECK all values within domain.
  context->float_array_map[identifier] = *assignments;
  delete assignments;
  delete annotations;
}
#line 1500 "./ortools/flatzinc/parser.tab.cc" /* yacc.c:1646  */
    break;

  case 23:
#line 210 "./ortools/flatzinc/parser.yy" /* yacc.c:1646  */
    {
  std::vector<Annotation>* const annotations = (yyvsp[-3].annotations);
  // Declaration of a (named) constant array. See rule right above.
  CHECK_EQ((yyvsp[-11].integer_value), 1) << "Only [1..n] array are supported here.";
  const int64 num_constants = (yyvsp[-9].integer_value);
  CHECK_EQ(num_constants, 0) << "Empty arrays should have a size of 0";
  const std::string& identifier = (yyvsp[-4].string_value);
  context->float_array_map[identifier] = std::vector<double>();
  delete annotations;
}
#line 1515 "./ortools/flatzinc/parser.tab.cc" /* yacc.c:1646  */
    break;

  case 24:
#line 221 "./ortools/flatzinc/parser.yy" /* yacc.c:1646  */
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
#line 1545 "./ortools/flatzinc/parser.tab.cc" /* yacc.c:1646  */
    break;

  case 25:
#line 246 "./ortools/flatzinc/parser.yy" /* yacc.c:1646  */
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
#line 1583 "./ortools/flatzinc/parser.tab.cc" /* yacc.c:1646  */
    break;

  case 26:
#line 280 "./ortools/flatzinc/parser.yy" /* yacc.c:1646  */
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
#line 1655 "./ortools/flatzinc/parser.tab.cc" /* yacc.c:1646  */
    break;

  case 27:
#line 349 "./ortools/flatzinc/parser.yy" /* yacc.c:1646  */
    { (yyval.var_or_value) = (yyvsp[0].var_or_value); }
#line 1661 "./ortools/flatzinc/parser.tab.cc" /* yacc.c:1646  */
    break;

  case 28:
#line 350 "./ortools/flatzinc/parser.yy" /* yacc.c:1646  */
    { (yyval.var_or_value) = VariableRefOrValue::Undefined(); }
#line 1667 "./ortools/flatzinc/parser.tab.cc" /* yacc.c:1646  */
    break;

  case 29:
#line 353 "./ortools/flatzinc/parser.yy" /* yacc.c:1646  */
    { (yyval.var_or_value_array) = (yyvsp[-1].var_or_value_array); }
#line 1673 "./ortools/flatzinc/parser.tab.cc" /* yacc.c:1646  */
    break;

  case 30:
#line 354 "./ortools/flatzinc/parser.yy" /* yacc.c:1646  */
    { (yyval.var_or_value_array) = nullptr; }
#line 1679 "./ortools/flatzinc/parser.tab.cc" /* yacc.c:1646  */
    break;

  case 31:
#line 355 "./ortools/flatzinc/parser.yy" /* yacc.c:1646  */
    { (yyval.var_or_value_array) = nullptr; }
#line 1685 "./ortools/flatzinc/parser.tab.cc" /* yacc.c:1646  */
    break;

  case 32:
#line 358 "./ortools/flatzinc/parser.yy" /* yacc.c:1646  */
    {
  (yyval.var_or_value_array) = (yyvsp[-2].var_or_value_array);
  (yyval.var_or_value_array)->PushBack((yyvsp[0].var_or_value));
}
#line 1694 "./ortools/flatzinc/parser.tab.cc" /* yacc.c:1646  */
    break;

  case 33:
#line 362 "./ortools/flatzinc/parser.yy" /* yacc.c:1646  */
    {
  (yyval.var_or_value_array) = new VariableRefOrValueArray();
  (yyval.var_or_value_array)->PushBack((yyvsp[0].var_or_value));
}
#line 1703 "./ortools/flatzinc/parser.tab.cc" /* yacc.c:1646  */
    break;

  case 34:
#line 368 "./ortools/flatzinc/parser.yy" /* yacc.c:1646  */
    { (yyval.var_or_value) = VariableRefOrValue::Value((yyvsp[0].integer_value)); }
#line 1709 "./ortools/flatzinc/parser.tab.cc" /* yacc.c:1646  */
    break;

  case 35:
#line 369 "./ortools/flatzinc/parser.yy" /* yacc.c:1646  */
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
#line 1727 "./ortools/flatzinc/parser.tab.cc" /* yacc.c:1646  */
    break;

  case 36:
#line 382 "./ortools/flatzinc/parser.yy" /* yacc.c:1646  */
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
#line 1748 "./ortools/flatzinc/parser.tab.cc" /* yacc.c:1646  */
    break;

  case 37:
#line 400 "./ortools/flatzinc/parser.yy" /* yacc.c:1646  */
    { (yyval.domain) = Domain::Boolean(); }
#line 1754 "./ortools/flatzinc/parser.tab.cc" /* yacc.c:1646  */
    break;

  case 38:
#line 401 "./ortools/flatzinc/parser.yy" /* yacc.c:1646  */
    { (yyval.domain) = Domain::AllInt64(); }
#line 1760 "./ortools/flatzinc/parser.tab.cc" /* yacc.c:1646  */
    break;

  case 39:
#line 402 "./ortools/flatzinc/parser.yy" /* yacc.c:1646  */
    { (yyval.domain) = Domain::Interval((yyvsp[-2].integer_value), (yyvsp[0].integer_value)); }
#line 1766 "./ortools/flatzinc/parser.tab.cc" /* yacc.c:1646  */
    break;

  case 40:
#line 403 "./ortools/flatzinc/parser.yy" /* yacc.c:1646  */
    {
  CHECK((yyvsp[-1].integers) != nullptr);
  (yyval.domain) = Domain::IntegerList(std::move(*(yyvsp[-1].integers)));
  delete (yyvsp[-1].integers);
}
#line 1776 "./ortools/flatzinc/parser.tab.cc" /* yacc.c:1646  */
    break;

  case 41:
#line 410 "./ortools/flatzinc/parser.yy" /* yacc.c:1646  */
    { (yyval.domain) = Domain::SetOfBoolean(); }
#line 1782 "./ortools/flatzinc/parser.tab.cc" /* yacc.c:1646  */
    break;

  case 42:
#line 411 "./ortools/flatzinc/parser.yy" /* yacc.c:1646  */
    { (yyval.domain) = Domain::SetOfAllInt64(); }
#line 1788 "./ortools/flatzinc/parser.tab.cc" /* yacc.c:1646  */
    break;

  case 43:
#line 412 "./ortools/flatzinc/parser.yy" /* yacc.c:1646  */
    { (yyval.domain) = Domain::SetOfInterval((yyvsp[-2].integer_value), (yyvsp[0].integer_value)); }
#line 1794 "./ortools/flatzinc/parser.tab.cc" /* yacc.c:1646  */
    break;

  case 44:
#line 413 "./ortools/flatzinc/parser.yy" /* yacc.c:1646  */
    {
  CHECK((yyvsp[-1].integers) != nullptr);
  (yyval.domain) = Domain::SetOfIntegerList(std::move(*(yyvsp[-1].integers)));
  delete (yyvsp[-1].integers);
}
#line 1804 "./ortools/flatzinc/parser.tab.cc" /* yacc.c:1646  */
    break;

  case 45:
#line 420 "./ortools/flatzinc/parser.yy" /* yacc.c:1646  */
    { (yyval.domain) = Domain::AllInt64(); }
#line 1810 "./ortools/flatzinc/parser.tab.cc" /* yacc.c:1646  */
    break;

  case 46:
#line 421 "./ortools/flatzinc/parser.yy" /* yacc.c:1646  */
    {
  const int64 lb = ConvertAsIntegerOrDie((yyvsp[-2].double_value));
  const int64 ub = ConvertAsIntegerOrDie((yyvsp[0].double_value));
  (yyval.domain) = Domain::Interval(lb, ub);
}
#line 1820 "./ortools/flatzinc/parser.tab.cc" /* yacc.c:1646  */
    break;

  case 47:
#line 428 "./ortools/flatzinc/parser.yy" /* yacc.c:1646  */
    { (yyval.domain) = (yyvsp[0].domain); }
#line 1826 "./ortools/flatzinc/parser.tab.cc" /* yacc.c:1646  */
    break;

  case 48:
#line 429 "./ortools/flatzinc/parser.yy" /* yacc.c:1646  */
    { (yyval.domain) = (yyvsp[0].domain); }
#line 1832 "./ortools/flatzinc/parser.tab.cc" /* yacc.c:1646  */
    break;

  case 49:
#line 430 "./ortools/flatzinc/parser.yy" /* yacc.c:1646  */
    { (yyval.domain) = (yyvsp[0].domain); }
#line 1838 "./ortools/flatzinc/parser.tab.cc" /* yacc.c:1646  */
    break;

  case 50:
#line 433 "./ortools/flatzinc/parser.yy" /* yacc.c:1646  */
    { (yyval.integers) = (yyvsp[-2].integers); (yyval.integers)->emplace_back((yyvsp[0].integer_value)); }
#line 1844 "./ortools/flatzinc/parser.tab.cc" /* yacc.c:1646  */
    break;

  case 51:
#line 434 "./ortools/flatzinc/parser.yy" /* yacc.c:1646  */
    { (yyval.integers) = new std::vector<int64>(); (yyval.integers)->emplace_back((yyvsp[0].integer_value)); }
#line 1850 "./ortools/flatzinc/parser.tab.cc" /* yacc.c:1646  */
    break;

  case 52:
#line 437 "./ortools/flatzinc/parser.yy" /* yacc.c:1646  */
    { (yyval.integer_value) = (yyvsp[0].integer_value); }
#line 1856 "./ortools/flatzinc/parser.tab.cc" /* yacc.c:1646  */
    break;

  case 53:
#line 438 "./ortools/flatzinc/parser.yy" /* yacc.c:1646  */
    { (yyval.integer_value) = FindOrDie(context->integer_map, (yyvsp[0].string_value)); }
#line 1862 "./ortools/flatzinc/parser.tab.cc" /* yacc.c:1646  */
    break;

  case 54:
#line 439 "./ortools/flatzinc/parser.yy" /* yacc.c:1646  */
    {
  (yyval.integer_value) = Lookup(FindOrDie(context->integer_array_map, (yyvsp[-3].string_value)), (yyvsp[-1].integer_value));
}
#line 1870 "./ortools/flatzinc/parser.tab.cc" /* yacc.c:1646  */
    break;

  case 55:
#line 444 "./ortools/flatzinc/parser.yy" /* yacc.c:1646  */
    { (yyval.doubles) = (yyvsp[-2].doubles); (yyval.doubles)->emplace_back((yyvsp[0].double_value)); }
#line 1876 "./ortools/flatzinc/parser.tab.cc" /* yacc.c:1646  */
    break;

  case 56:
#line 445 "./ortools/flatzinc/parser.yy" /* yacc.c:1646  */
    { (yyval.doubles) = new std::vector<double>(); (yyval.doubles)->emplace_back((yyvsp[0].double_value)); }
#line 1882 "./ortools/flatzinc/parser.tab.cc" /* yacc.c:1646  */
    break;

  case 57:
#line 448 "./ortools/flatzinc/parser.yy" /* yacc.c:1646  */
    { (yyval.double_value) = (yyvsp[0].double_value); }
#line 1888 "./ortools/flatzinc/parser.tab.cc" /* yacc.c:1646  */
    break;

  case 58:
#line 449 "./ortools/flatzinc/parser.yy" /* yacc.c:1646  */
    { (yyval.double_value) = FindOrDie(context->float_map, (yyvsp[0].string_value)); }
#line 1894 "./ortools/flatzinc/parser.tab.cc" /* yacc.c:1646  */
    break;

  case 59:
#line 450 "./ortools/flatzinc/parser.yy" /* yacc.c:1646  */
    {
  (yyval.double_value) = Lookup(FindOrDie(context->float_array_map, (yyvsp[-3].string_value)), (yyvsp[-1].integer_value));
}
#line 1902 "./ortools/flatzinc/parser.tab.cc" /* yacc.c:1646  */
    break;

  case 60:
#line 455 "./ortools/flatzinc/parser.yy" /* yacc.c:1646  */
    { (yyval.domain) = Domain::IntegerValue((yyvsp[0].integer_value)); }
#line 1908 "./ortools/flatzinc/parser.tab.cc" /* yacc.c:1646  */
    break;

  case 61:
#line 456 "./ortools/flatzinc/parser.yy" /* yacc.c:1646  */
    { (yyval.domain) = Domain::Interval((yyvsp[-2].integer_value), (yyvsp[0].integer_value)); }
#line 1914 "./ortools/flatzinc/parser.tab.cc" /* yacc.c:1646  */
    break;

  case 62:
#line 457 "./ortools/flatzinc/parser.yy" /* yacc.c:1646  */
    {
  CHECK((yyvsp[-1].integers) != nullptr);
  (yyval.domain) = Domain::IntegerList(std::move(*(yyvsp[-1].integers)));
  delete (yyvsp[-1].integers);
}
#line 1924 "./ortools/flatzinc/parser.tab.cc" /* yacc.c:1646  */
    break;

  case 63:
#line 462 "./ortools/flatzinc/parser.yy" /* yacc.c:1646  */
    { (yyval.domain) = Domain::EmptyDomain(); }
#line 1930 "./ortools/flatzinc/parser.tab.cc" /* yacc.c:1646  */
    break;

  case 64:
#line 463 "./ortools/flatzinc/parser.yy" /* yacc.c:1646  */
    {
  CHECK_EQ(std::round((yyvsp[0].double_value)), (yyvsp[0].double_value));
  (yyval.domain) = Domain::IntegerValue(static_cast<int64>((yyvsp[0].double_value)));
}
#line 1939 "./ortools/flatzinc/parser.tab.cc" /* yacc.c:1646  */
    break;

  case 65:
#line 467 "./ortools/flatzinc/parser.yy" /* yacc.c:1646  */
    { (yyval.domain) = Domain::IntegerValue(FindOrDie(context->integer_map, (yyvsp[0].string_value))); }
#line 1945 "./ortools/flatzinc/parser.tab.cc" /* yacc.c:1646  */
    break;

  case 66:
#line 468 "./ortools/flatzinc/parser.yy" /* yacc.c:1646  */
    {
  (yyval.domain) = Domain::IntegerValue(
      Lookup(FindOrDie(context->integer_array_map, (yyvsp[-3].string_value)), (yyvsp[-1].integer_value)));
}
#line 1954 "./ortools/flatzinc/parser.tab.cc" /* yacc.c:1646  */
    break;

  case 67:
#line 474 "./ortools/flatzinc/parser.yy" /* yacc.c:1646  */
    {
  (yyval.domains) = (yyvsp[-2].domains);
  (yyval.domains)->emplace_back((yyvsp[0].domain));
}
#line 1963 "./ortools/flatzinc/parser.tab.cc" /* yacc.c:1646  */
    break;

  case 68:
#line 478 "./ortools/flatzinc/parser.yy" /* yacc.c:1646  */
    { (yyval.domains) = new std::vector<Domain>(); (yyval.domains)->emplace_back((yyvsp[0].domain)); }
#line 1969 "./ortools/flatzinc/parser.tab.cc" /* yacc.c:1646  */
    break;

  case 71:
#line 488 "./ortools/flatzinc/parser.yy" /* yacc.c:1646  */
    {
  const std::string& identifier = (yyvsp[-4].string_value);
  CHECK((yyvsp[-2].args) != nullptr) << "Missing argument in constraint";
  const std::vector<Argument>& arguments = *(yyvsp[-2].args);
  std::vector<Annotation>* const annotations = (yyvsp[0].annotations);

  // Does the constraint have a defines_var annotation?
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
#line 1999 "./ortools/flatzinc/parser.tab.cc" /* yacc.c:1646  */
    break;

  case 72:
#line 515 "./ortools/flatzinc/parser.yy" /* yacc.c:1646  */
    { (yyval.args) = (yyvsp[-2].args); (yyval.args)->emplace_back((yyvsp[0].arg)); }
#line 2005 "./ortools/flatzinc/parser.tab.cc" /* yacc.c:1646  */
    break;

  case 73:
#line 516 "./ortools/flatzinc/parser.yy" /* yacc.c:1646  */
    { (yyval.args) = new std::vector<Argument>(); (yyval.args)->emplace_back((yyvsp[0].arg)); }
#line 2011 "./ortools/flatzinc/parser.tab.cc" /* yacc.c:1646  */
    break;

  case 74:
#line 519 "./ortools/flatzinc/parser.yy" /* yacc.c:1646  */
    { (yyval.arg) = Argument::IntegerValue((yyvsp[0].integer_value)); }
#line 2017 "./ortools/flatzinc/parser.tab.cc" /* yacc.c:1646  */
    break;

  case 75:
#line 520 "./ortools/flatzinc/parser.yy" /* yacc.c:1646  */
    { (yyval.arg) = Argument::IntegerValue(ConvertAsIntegerOrDie((yyvsp[0].double_value))); }
#line 2023 "./ortools/flatzinc/parser.tab.cc" /* yacc.c:1646  */
    break;

  case 76:
#line 521 "./ortools/flatzinc/parser.yy" /* yacc.c:1646  */
    { (yyval.arg) = Argument::VoidArgument(); }
#line 2029 "./ortools/flatzinc/parser.tab.cc" /* yacc.c:1646  */
    break;

  case 77:
#line 522 "./ortools/flatzinc/parser.yy" /* yacc.c:1646  */
    { (yyval.arg) = Argument::Interval((yyvsp[-2].integer_value), (yyvsp[0].integer_value)); }
#line 2035 "./ortools/flatzinc/parser.tab.cc" /* yacc.c:1646  */
    break;

  case 78:
#line 523 "./ortools/flatzinc/parser.yy" /* yacc.c:1646  */
    {
  CHECK((yyvsp[-1].integers) != nullptr);
  (yyval.arg) = Argument::IntegerList(std::move(*(yyvsp[-1].integers)));
  delete (yyvsp[-1].integers);
}
#line 2045 "./ortools/flatzinc/parser.tab.cc" /* yacc.c:1646  */
    break;

  case 79:
#line 528 "./ortools/flatzinc/parser.yy" /* yacc.c:1646  */
    {
  const std::string& id = (yyvsp[0].string_value);
  if (ContainsKey(context->integer_map, id)) {
    (yyval.arg) = Argument::IntegerValue(FindOrDie(context->integer_map, id));
  } else if (ContainsKey(context->integer_array_map, id)) {
    (yyval.arg) = Argument::IntegerList(FindOrDie(context->integer_array_map, id));
  } else if (ContainsKey(context->float_map, id)) {
    const double d = FindOrDie(context->float_map, id);
    (yyval.arg) = Argument::IntegerValue(ConvertAsIntegerOrDie(d));
  } else if (ContainsKey(context->float_array_map, id)) {
    const auto& double_values = FindOrDie(context->float_array_map, id);
    std::vector<int64> integer_values;
    for (const double d : double_values) {
      const int64 i = ConvertAsIntegerOrDie(d);
      integer_values.push_back(i);
    }
    (yyval.arg) = Argument::IntegerList(std::move(integer_values));
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
#line 2081 "./ortools/flatzinc/parser.tab.cc" /* yacc.c:1646  */
    break;

  case 80:
#line 559 "./ortools/flatzinc/parser.yy" /* yacc.c:1646  */
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
#line 2103 "./ortools/flatzinc/parser.tab.cc" /* yacc.c:1646  */
    break;

  case 81:
#line 576 "./ortools/flatzinc/parser.yy" /* yacc.c:1646  */
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
#line 2133 "./ortools/flatzinc/parser.tab.cc" /* yacc.c:1646  */
    break;

  case 82:
#line 601 "./ortools/flatzinc/parser.yy" /* yacc.c:1646  */
    {
  (yyval.arg) = Argument::VoidArgument();
}
#line 2141 "./ortools/flatzinc/parser.tab.cc" /* yacc.c:1646  */
    break;

  case 83:
#line 610 "./ortools/flatzinc/parser.yy" /* yacc.c:1646  */
    {
    (yyval.annotations) = (yyvsp[-2].annotations) != nullptr ? (yyvsp[-2].annotations) : new std::vector<Annotation>();
    (yyval.annotations)->emplace_back((yyvsp[0].annotation));
  }
#line 2150 "./ortools/flatzinc/parser.tab.cc" /* yacc.c:1646  */
    break;

  case 84:
#line 614 "./ortools/flatzinc/parser.yy" /* yacc.c:1646  */
    { (yyval.annotations) = nullptr; }
#line 2156 "./ortools/flatzinc/parser.tab.cc" /* yacc.c:1646  */
    break;

  case 85:
#line 617 "./ortools/flatzinc/parser.yy" /* yacc.c:1646  */
    { (yyval.annotations) = (yyvsp[-2].annotations); (yyval.annotations)->emplace_back((yyvsp[0].annotation)); }
#line 2162 "./ortools/flatzinc/parser.tab.cc" /* yacc.c:1646  */
    break;

  case 86:
#line 618 "./ortools/flatzinc/parser.yy" /* yacc.c:1646  */
    { (yyval.annotations) = new std::vector<Annotation>(); (yyval.annotations)->emplace_back((yyvsp[0].annotation)); }
#line 2168 "./ortools/flatzinc/parser.tab.cc" /* yacc.c:1646  */
    break;

  case 87:
#line 621 "./ortools/flatzinc/parser.yy" /* yacc.c:1646  */
    { (yyval.annotation) = Annotation::Interval((yyvsp[-2].integer_value), (yyvsp[0].integer_value)); }
#line 2174 "./ortools/flatzinc/parser.tab.cc" /* yacc.c:1646  */
    break;

  case 88:
#line 622 "./ortools/flatzinc/parser.yy" /* yacc.c:1646  */
    { (yyval.annotation) = Annotation::IntegerValue((yyvsp[0].integer_value)); }
#line 2180 "./ortools/flatzinc/parser.tab.cc" /* yacc.c:1646  */
    break;

  case 89:
#line 623 "./ortools/flatzinc/parser.yy" /* yacc.c:1646  */
    { (yyval.annotation) = Annotation::String((yyvsp[0].string_value)); }
#line 2186 "./ortools/flatzinc/parser.tab.cc" /* yacc.c:1646  */
    break;

  case 90:
#line 624 "./ortools/flatzinc/parser.yy" /* yacc.c:1646  */
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
#line 2201 "./ortools/flatzinc/parser.tab.cc" /* yacc.c:1646  */
    break;

  case 91:
#line 634 "./ortools/flatzinc/parser.yy" /* yacc.c:1646  */
    {
  std::vector<Annotation>* const annotations = (yyvsp[-1].annotations);
  if (annotations != nullptr) {
    (yyval.annotation) = Annotation::FunctionCallWithArguments((yyvsp[-3].string_value), std::move(*annotations));
    delete annotations;
  } else {
    (yyval.annotation) = Annotation::FunctionCall((yyvsp[-3].string_value));
  }
}
#line 2215 "./ortools/flatzinc/parser.tab.cc" /* yacc.c:1646  */
    break;

  case 92:
#line 643 "./ortools/flatzinc/parser.yy" /* yacc.c:1646  */
    {
  CHECK(ContainsKey(context->variable_array_map, (yyvsp[-3].string_value)))
      << "Unknown identifier: " << (yyvsp[-3].string_value);
  (yyval.annotation) = Annotation::Variable(
      Lookup(FindOrDie(context->variable_array_map, (yyvsp[-3].string_value)), (yyvsp[-1].integer_value)));
}
#line 2226 "./ortools/flatzinc/parser.tab.cc" /* yacc.c:1646  */
    break;

  case 93:
#line 649 "./ortools/flatzinc/parser.yy" /* yacc.c:1646  */
    {
  std::vector<Annotation>* const annotations = (yyvsp[-1].annotations);
  if (annotations != nullptr) {
    (yyval.annotation) = Annotation::AnnotationList(std::move(*annotations));
    delete annotations;
  } else {
    (yyval.annotation) = Annotation::Empty();
  }
}
#line 2240 "./ortools/flatzinc/parser.tab.cc" /* yacc.c:1646  */
    break;

  case 94:
#line 664 "./ortools/flatzinc/parser.yy" /* yacc.c:1646  */
    {
  if ((yyvsp[-1].annotations) != nullptr) {
    model->Satisfy(std::move(*(yyvsp[-1].annotations)));
    delete (yyvsp[-1].annotations);
  } else {
    model->Satisfy(std::vector<Annotation>());
  }
}
#line 2253 "./ortools/flatzinc/parser.tab.cc" /* yacc.c:1646  */
    break;

  case 95:
#line 672 "./ortools/flatzinc/parser.yy" /* yacc.c:1646  */
    {
  CHECK_EQ(Argument::INT_VAR_REF, (yyvsp[0].arg).type);
  if ((yyvsp[-2].annotations) != nullptr) {
    model->Minimize((yyvsp[0].arg).Var(), std::move(*(yyvsp[-2].annotations)));
    delete (yyvsp[-2].annotations);
  } else {
    model->Minimize((yyvsp[0].arg).Var(), std::vector<Annotation>());
  }
}
#line 2267 "./ortools/flatzinc/parser.tab.cc" /* yacc.c:1646  */
    break;

  case 96:
#line 681 "./ortools/flatzinc/parser.yy" /* yacc.c:1646  */
    {
  CHECK_EQ(Argument::INT_VAR_REF, (yyvsp[0].arg).type);
  if ((yyvsp[-2].annotations) != nullptr) {
    model->Maximize((yyvsp[0].arg).Var(), std::move(*(yyvsp[-2].annotations)));
    delete (yyvsp[-2].annotations);
  } else {
    model->Maximize((yyvsp[0].arg).Var(), std::vector<Annotation>());
  }
}
#line 2281 "./ortools/flatzinc/parser.tab.cc" /* yacc.c:1646  */
    break;


#line 2285 "./ortools/flatzinc/parser.tab.cc" /* yacc.c:1646  */
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
#line 691 "./ortools/flatzinc/parser.yy" /* yacc.c:1906  */

