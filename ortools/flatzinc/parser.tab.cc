/* A Bison parser, made by GNU Bison 3.8.2.  */

/* Bison implementation for Yacc-like parsers in C

   Copyright (C) 1984, 1989-1990, 2000-2015, 2018-2021 Free Software Foundation,
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
   along with this program.  If not, see <https://www.gnu.org/licenses/>.  */

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

/* DO NOT RELY ON FEATURES THAT ARE NOT DOCUMENTED in the manual,
   especially those whose name start with YY_ or yy_.  They are
   private implementation details that can be changed or removed.  */

/* All symbols defined below should begin with yy or YY, to avoid
   infringing on user name space.  This should be done even for local
   variables, as they might otherwise be expanded by user macros.
   There are some unavoidable exceptions within include files to
   define necessary library symbols; they are noted "INFRINGES ON
   USER NAME SPACE" below.  */

/* Identify Bison output, and Bison version.  */
#define YYBISON 30802

/* Bison version string.  */
#define YYBISON_VERSION "3.8.2"

/* Skeleton name.  */
#define YYSKELETON_NAME "yacc.c"

/* Pure parsers.  */
#define YYPURE 2

/* Push parsers.  */
#define YYPUSH 0

/* Pull parsers.  */
#define YYPULL 1

/* Substitute the type names.  */
#define YYSTYPE ORFZ_STYPE
/* Substitute the variable and function names.  */
#define yyparse orfz_parse
#define yylex orfz_lex
#define yyerror orfz_error
#define yydebug orfz_debug
#define yynerrs orfz_nerrs

#ifndef YY_CAST
#ifdef __cplusplus
#define YY_CAST(Type, Val) static_cast<Type>(Val)
#define YY_REINTERPRET_CAST(Type, Val) reinterpret_cast<Type>(Val)
#else
#define YY_CAST(Type, Val) ((Type)(Val))
#define YY_REINTERPRET_CAST(Type, Val) ((Type)(Val))
#endif
#endif
#ifndef YY_NULLPTR
#if defined __cplusplus
#if 201103L <= __cplusplus
#define YY_NULLPTR nullptr
#else
#define YY_NULLPTR 0
#endif
#else
#define YY_NULLPTR ((void*)0)
#endif
#endif

#include "parser.tab.hh"
/* Symbol kind.  */
enum yysymbol_kind_t {
  YYSYMBOL_YYEMPTY = -2,
  YYSYMBOL_YYEOF = 0,                     /* "end of file"  */
  YYSYMBOL_YYerror = 1,                   /* error  */
  YYSYMBOL_YYUNDEF = 2,                   /* "invalid token"  */
  YYSYMBOL_ARRAY = 3,                     /* ARRAY  */
  YYSYMBOL_TOKEN_BOOL = 4,                /* TOKEN_BOOL  */
  YYSYMBOL_CONSTRAINT = 5,                /* CONSTRAINT  */
  YYSYMBOL_TOKEN_FLOAT = 6,               /* TOKEN_FLOAT  */
  YYSYMBOL_TOKEN_INT = 7,                 /* TOKEN_INT  */
  YYSYMBOL_MAXIMIZE = 8,                  /* MAXIMIZE  */
  YYSYMBOL_MINIMIZE = 9,                  /* MINIMIZE  */
  YYSYMBOL_OF = 10,                       /* OF  */
  YYSYMBOL_PREDICATE = 11,                /* PREDICATE  */
  YYSYMBOL_SATISFY = 12,                  /* SATISFY  */
  YYSYMBOL_SET = 13,                      /* SET  */
  YYSYMBOL_SOLVE = 14,                    /* SOLVE  */
  YYSYMBOL_VAR = 15,                      /* VAR  */
  YYSYMBOL_DOTDOT = 16,                   /* DOTDOT  */
  YYSYMBOL_COLONCOLON = 17,               /* COLONCOLON  */
  YYSYMBOL_IVALUE = 18,                   /* IVALUE  */
  YYSYMBOL_SVALUE = 19,                   /* SVALUE  */
  YYSYMBOL_IDENTIFIER = 20,               /* IDENTIFIER  */
  YYSYMBOL_DVALUE = 21,                   /* DVALUE  */
  YYSYMBOL_22_ = 22,                      /* ';'  */
  YYSYMBOL_23_ = 23,                      /* '('  */
  YYSYMBOL_24_ = 24,                      /* ')'  */
  YYSYMBOL_25_ = 25,                      /* ','  */
  YYSYMBOL_26_ = 26,                      /* ':'  */
  YYSYMBOL_27_ = 27,                      /* '['  */
  YYSYMBOL_28_ = 28,                      /* ']'  */
  YYSYMBOL_29_ = 29,                      /* '='  */
  YYSYMBOL_30_ = 30,                      /* '{'  */
  YYSYMBOL_31_ = 31,                      /* '}'  */
  YYSYMBOL_YYACCEPT = 32,                 /* $accept  */
  YYSYMBOL_model = 33,                    /* model  */
  YYSYMBOL_predicates = 34,               /* predicates  */
  YYSYMBOL_predicate = 35,                /* predicate  */
  YYSYMBOL_predicate_arguments = 36,      /* predicate_arguments  */
  YYSYMBOL_predicate_argument = 37,       /* predicate_argument  */
  YYSYMBOL_predicate_array_argument = 38, /* predicate_array_argument  */
  YYSYMBOL_predicate_ints = 39,           /* predicate_ints  */
  YYSYMBOL_variable_or_constant_declarations =
      40, /* variable_or_constant_declarations  */
  YYSYMBOL_variable_or_constant_declaration =
      41,                              /* variable_or_constant_declaration  */
  YYSYMBOL_optional_var_or_value = 42, /* optional_var_or_value  */
  YYSYMBOL_optional_var_or_value_array = 43, /* optional_var_or_value_array  */
  YYSYMBOL_var_or_value_array = 44,          /* var_or_value_array  */
  YYSYMBOL_var_or_value = 45,                /* var_or_value  */
  YYSYMBOL_int_domain = 46,                  /* int_domain  */
  YYSYMBOL_set_domain = 47,                  /* set_domain  */
  YYSYMBOL_float_domain = 48,                /* float_domain  */
  YYSYMBOL_domain = 49,                      /* domain  */
  YYSYMBOL_integers = 50,                    /* integers  */
  YYSYMBOL_integer = 51,                     /* integer  */
  YYSYMBOL_floats = 52,                      /* floats  */
  YYSYMBOL_float = 53,                       /* float  */
  YYSYMBOL_const_literal = 54,               /* const_literal  */
  YYSYMBOL_const_literals = 55,              /* const_literals  */
  YYSYMBOL_constraints = 56,                 /* constraints  */
  YYSYMBOL_constraint = 57,                  /* constraint  */
  YYSYMBOL_arguments = 58,                   /* arguments  */
  YYSYMBOL_argument = 59,                    /* argument  */
  YYSYMBOL_annotations = 60,                 /* annotations  */
  YYSYMBOL_annotation_arguments = 61,        /* annotation_arguments  */
  YYSYMBOL_annotation = 62,                  /* annotation  */
  YYSYMBOL_solve = 63                        /* solve  */
};
typedef enum yysymbol_kind_t yysymbol_kind_t;

/* Unqualified %code blocks.  */
#line 36 "ortools/flatzinc/parser.yy"

#include <cstdint>

#include "absl/log/check.h"
#include "absl/strings/match.h"
#include "absl/strings/str_format.h"
#include "ortools/flatzinc/parser_util.cc"

using operations_research::fz::Annotation;
using operations_research::fz::Argument;
using operations_research::fz::Constraint;
using operations_research::fz::ConvertAsIntegerOrDie;
using operations_research::fz::Domain;
using operations_research::fz::Lookup;
using operations_research::fz::Model;
using operations_research::fz::ParserContext;
using operations_research::fz::SolutionOutputSpecs;
using operations_research::fz::Variable;
using operations_research::fz::VarRefOrValue;

#line 194 "parser.tab.cc"

#ifdef short
#undef short
#endif

/* On compilers that do not define __PTRDIFF_MAX__ etc., make sure
   <limits.h> and (if available) <stdint.h> are included
   so that the code can choose integer types of a good width.  */

#ifndef __PTRDIFF_MAX__
#include <limits.h> /* INFRINGES ON USER NAME SPACE */
#if defined __STDC_VERSION__ && 199901 <= __STDC_VERSION__
#include <stdint.h> /* INFRINGES ON USER NAME SPACE */
#define YY_STDINT_H
#endif
#endif

/* Narrow types that promote to a signed type and that can represent a
   signed or unsigned integer of at least N bits.  In tables they can
   save space and decrease cache pressure.  Promoting to a signed type
   helps avoid bugs in integer arithmetic.  */

#ifdef __INT_LEAST8_MAX__
typedef __INT_LEAST8_TYPE__ yytype_int8;
#elif defined YY_STDINT_H
typedef int_least8_t yytype_int8;
#else
typedef signed char yytype_int8;
#endif

#ifdef __INT_LEAST16_MAX__
typedef __INT_LEAST16_TYPE__ yytype_int16;
#elif defined YY_STDINT_H
typedef int_least16_t yytype_int16;
#else
typedef short yytype_int16;
#endif

/* Work around bug in HP-UX 11.23, which defines these macros
   incorrectly for preprocessor constants.  This workaround can likely
   be removed in 2023, as HPE has promised support for HP-UX 11.23
   (aka HP-UX 11i v2) only through the end of 2022; see Table 2 of
   <https://h20195.www2.hpe.com/V2/getpdf.aspx/4AA4-7673ENW.pdf>.  */
#ifdef __hpux
#undef UINT_LEAST8_MAX
#undef UINT_LEAST16_MAX
#define UINT_LEAST8_MAX 255
#define UINT_LEAST16_MAX 65535
#endif

#if defined __UINT_LEAST8_MAX__ && __UINT_LEAST8_MAX__ <= __INT_MAX__
typedef __UINT_LEAST8_TYPE__ yytype_uint8;
#elif (!defined __UINT_LEAST8_MAX__ && defined YY_STDINT_H && \
       UINT_LEAST8_MAX <= INT_MAX)
typedef uint_least8_t yytype_uint8;
#elif !defined __UINT_LEAST8_MAX__ && UCHAR_MAX <= INT_MAX
typedef unsigned char yytype_uint8;
#else
typedef short yytype_uint8;
#endif

#if defined __UINT_LEAST16_MAX__ && __UINT_LEAST16_MAX__ <= __INT_MAX__
typedef __UINT_LEAST16_TYPE__ yytype_uint16;
#elif (!defined __UINT_LEAST16_MAX__ && defined YY_STDINT_H && \
       UINT_LEAST16_MAX <= INT_MAX)
typedef uint_least16_t yytype_uint16;
#elif !defined __UINT_LEAST16_MAX__ && USHRT_MAX <= INT_MAX
typedef unsigned short yytype_uint16;
#else
typedef int yytype_uint16;
#endif

#ifndef YYPTRDIFF_T
#if defined __PTRDIFF_TYPE__ && defined __PTRDIFF_MAX__
#define YYPTRDIFF_T __PTRDIFF_TYPE__
#define YYPTRDIFF_MAXIMUM __PTRDIFF_MAX__
#elif defined PTRDIFF_MAX
#ifndef ptrdiff_t
#include <stddef.h> /* INFRINGES ON USER NAME SPACE */
#endif
#define YYPTRDIFF_T ptrdiff_t
#define YYPTRDIFF_MAXIMUM PTRDIFF_MAX
#else
#define YYPTRDIFF_T long
#define YYPTRDIFF_MAXIMUM LONG_MAX
#endif
#endif

#ifndef YYSIZE_T
#ifdef __SIZE_TYPE__
#define YYSIZE_T __SIZE_TYPE__
#elif defined size_t
#define YYSIZE_T size_t
#elif defined __STDC_VERSION__ && 199901 <= __STDC_VERSION__
#include <stddef.h> /* INFRINGES ON USER NAME SPACE */
#define YYSIZE_T size_t
#else
#define YYSIZE_T unsigned
#endif
#endif

#define YYSIZE_MAXIMUM                                                   \
  YY_CAST(YYPTRDIFF_T,                                                   \
          (YYPTRDIFF_MAXIMUM < YY_CAST(YYSIZE_T, -1) ? YYPTRDIFF_MAXIMUM \
                                                     : YY_CAST(YYSIZE_T, -1)))

#define YYSIZEOF(X) YY_CAST(YYPTRDIFF_T, sizeof(X))

/* Stored state numbers (used for stacks). */
typedef yytype_uint8 yy_state_t;

/* State numbers in computations.  */
typedef int yy_state_fast_t;

#ifndef YY_
#if defined YYENABLE_NLS && YYENABLE_NLS
#if ENABLE_NLS
#include <libintl.h> /* INFRINGES ON USER NAME SPACE */
#define YY_(Msgid) dgettext("bison-runtime", Msgid)
#endif
#endif
#ifndef YY_
#define YY_(Msgid) Msgid
#endif
#endif

#ifndef YY_ATTRIBUTE_PURE
#if defined __GNUC__ && 2 < __GNUC__ + (96 <= __GNUC_MINOR__)
#define YY_ATTRIBUTE_PURE __attribute__((__pure__))
#else
#define YY_ATTRIBUTE_PURE
#endif
#endif

#ifndef YY_ATTRIBUTE_UNUSED
#if defined __GNUC__ && 2 < __GNUC__ + (7 <= __GNUC_MINOR__)
#define YY_ATTRIBUTE_UNUSED __attribute__((__unused__))
#else
#define YY_ATTRIBUTE_UNUSED
#endif
#endif

/* Suppress unused-variable warnings by "using" E.  */
#if !defined lint || defined __GNUC__
#define YY_USE(E) ((void)(E))
#else
#define YY_USE(E) /* empty */
#endif

/* Suppress an incorrect diagnostic about yylval being uninitialized.  */
#if defined __GNUC__ && !defined __ICC && 406 <= __GNUC__ * 100 + __GNUC_MINOR__
#if __GNUC__ * 100 + __GNUC_MINOR__ < 407
#define YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN \
  _Pragma("GCC diagnostic push")            \
      _Pragma("GCC diagnostic ignored \"-Wuninitialized\"")
#else
#define YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN                 \
  _Pragma("GCC diagnostic push")                            \
      _Pragma("GCC diagnostic ignored \"-Wuninitialized\"") \
          _Pragma("GCC diagnostic ignored \"-Wmaybe-uninitialized\"")
#endif
#define YY_IGNORE_MAYBE_UNINITIALIZED_END _Pragma("GCC diagnostic pop")
#else
#define YY_INITIAL_VALUE(Value) Value
#endif
#ifndef YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
#define YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
#define YY_IGNORE_MAYBE_UNINITIALIZED_END
#endif
#ifndef YY_INITIAL_VALUE
#define YY_INITIAL_VALUE(Value) /* Nothing. */
#endif

#if defined __cplusplus && defined __GNUC__ && !defined __ICC && 6 <= __GNUC__
#define YY_IGNORE_USELESS_CAST_BEGIN \
  _Pragma("GCC diagnostic push")     \
      _Pragma("GCC diagnostic ignored \"-Wuseless-cast\"")
#define YY_IGNORE_USELESS_CAST_END _Pragma("GCC diagnostic pop")
#endif
#ifndef YY_IGNORE_USELESS_CAST_BEGIN
#define YY_IGNORE_USELESS_CAST_BEGIN
#define YY_IGNORE_USELESS_CAST_END
#endif

#define YY_ASSERT(E) ((void)(0 && (E)))

#if 1

/* The parser invokes alloca or malloc; define the necessary symbols.  */

#ifdef YYSTACK_USE_ALLOCA
#if YYSTACK_USE_ALLOCA
#ifdef __GNUC__
#define YYSTACK_ALLOC __builtin_alloca
#elif defined __BUILTIN_VA_ARG_INCR
#include <alloca.h> /* INFRINGES ON USER NAME SPACE */
#elif defined _AIX
#define YYSTACK_ALLOC __alloca
#elif defined _MSC_VER
#include <malloc.h> /* INFRINGES ON USER NAME SPACE */
#define alloca _alloca
#else
#define YYSTACK_ALLOC alloca
#if !defined _ALLOCA_H && !defined EXIT_SUCCESS
#include <stdlib.h> /* INFRINGES ON USER NAME SPACE */
/* Use EXIT_SUCCESS as a witness for stdlib.h.  */
#ifndef EXIT_SUCCESS
#define EXIT_SUCCESS 0
#endif
#endif
#endif
#endif
#endif

#ifdef YYSTACK_ALLOC
/* Pacify GCC's 'empty if-body' warning.  */
#define YYSTACK_FREE(Ptr) \
  do { /* empty */        \
    ;                     \
  } while (0)
#ifndef YYSTACK_ALLOC_MAXIMUM
/* The OS might guarantee only one guard page at the bottom of the stack,
   and a page size can be as small as 4096 bytes.  So we cannot safely
   invoke alloca (N) if N exceeds 4096.  Use a slightly smaller number
   to allow for a few compiler-allocated temporary stack slots.  */
#define YYSTACK_ALLOC_MAXIMUM 4032 /* reasonable circa 2006 */
#endif
#else
#define YYSTACK_ALLOC YYMALLOC
#define YYSTACK_FREE YYFREE
#ifndef YYSTACK_ALLOC_MAXIMUM
#define YYSTACK_ALLOC_MAXIMUM YYSIZE_MAXIMUM
#endif
#if (defined __cplusplus && !defined EXIT_SUCCESS && \
     !((defined YYMALLOC || defined malloc) &&       \
       (defined YYFREE || defined free)))
#include <stdlib.h> /* INFRINGES ON USER NAME SPACE */
#ifndef EXIT_SUCCESS
#define EXIT_SUCCESS 0
#endif
#endif
#ifndef YYMALLOC
#define YYMALLOC malloc
#if !defined malloc && !defined EXIT_SUCCESS
void* malloc(YYSIZE_T); /* INFRINGES ON USER NAME SPACE */
#endif
#endif
#ifndef YYFREE
#define YYFREE free
#if !defined free && !defined EXIT_SUCCESS
void free(void*); /* INFRINGES ON USER NAME SPACE */
#endif
#endif
#endif
#endif /* 1 */

#if (!defined yyoverflow &&   \
     (!defined __cplusplus || \
      (defined ORFZ_STYPE_IS_TRIVIAL && ORFZ_STYPE_IS_TRIVIAL)))

/* A type that is properly aligned for any stack member.  */
union yyalloc {
  yy_state_t yyss_alloc;
  YYSTYPE yyvs_alloc;
};

/* The size of the maximum gap between one aligned stack and the next.  */
#define YYSTACK_GAP_MAXIMUM (YYSIZEOF(union yyalloc) - 1)

/* The size of an array large to enough to hold all stacks, each with
   N elements.  */
#define YYSTACK_BYTES(N) \
  ((N) * (YYSIZEOF(yy_state_t) + YYSIZEOF(YYSTYPE)) + YYSTACK_GAP_MAXIMUM)

#define YYCOPY_NEEDED 1

/* Relocate STACK from its old location to the new one.  The
   local variables YYSIZE and YYSTACKSIZE give the old and new number of
   elements in the stack, and YYPTR gives the new location of the
   stack.  Advance YYPTR to a properly aligned location for the next
   stack.  */
#define YYSTACK_RELOCATE(Stack_alloc, Stack)                           \
  do {                                                                 \
    YYPTRDIFF_T yynewbytes;                                            \
    YYCOPY(&yyptr->Stack_alloc, Stack, yysize);                        \
    Stack = &yyptr->Stack_alloc;                                       \
    yynewbytes = yystacksize * YYSIZEOF(*Stack) + YYSTACK_GAP_MAXIMUM; \
    yyptr += yynewbytes / YYSIZEOF(*yyptr);                            \
  } while (0)

#endif

#if defined YYCOPY_NEEDED && YYCOPY_NEEDED
/* Copy COUNT objects from SRC to DST.  The source and destination do
   not overlap.  */
#ifndef YYCOPY
#if defined __GNUC__ && 1 < __GNUC__
#define YYCOPY(Dst, Src, Count) \
  __builtin_memcpy(Dst, Src, YY_CAST(YYSIZE_T, (Count)) * sizeof(*(Src)))
#else
#define YYCOPY(Dst, Src, Count)                                  \
  do {                                                           \
    YYPTRDIFF_T yyi;                                             \
    for (yyi = 0; yyi < (Count); yyi++) (Dst)[yyi] = (Src)[yyi]; \
  } while (0)
#endif
#endif
#endif /* !YYCOPY_NEEDED */

/* YYFINAL -- State number of the termination state.  */
#define YYFINAL 3
/* YYLAST -- Last index in YYTABLE.  */
#define YYLAST 307

/* YYNTOKENS -- Number of terminals.  */
#define YYNTOKENS 32
/* YYNNTS -- Number of nonterminals.  */
#define YYNNTS 32
/* YYNRULES -- Number of rules.  */
#define YYNRULES 100
/* YYNSTATES -- Number of states.  */
#define YYNSTATES 230

/* YYMAXUTOK -- Last valid token kind.  */
#define YYMAXUTOK 276

/* YYTRANSLATE(TOKEN-NUM) -- Symbol number corresponding to TOKEN-NUM
   as returned by yylex, with out-of-bounds checking.  */
#define YYTRANSLATE(YYX)                            \
  (0 <= (YYX) && (YYX) <= YYMAXUTOK                 \
       ? YY_CAST(yysymbol_kind_t, yytranslate[YYX]) \
       : YYSYMBOL_YYUNDEF)

/* YYTRANSLATE[TOKEN-NUM] -- Symbol number corresponding to TOKEN-NUM
   as returned by yylex.  */
static const yytype_int8 yytranslate[] = {
    0,  2,  2, 2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2, 2, 2,  2,
    2,  2,  2, 2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2, 2, 2,  2,
    23, 24, 2, 2,  25, 2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2, 2, 26, 22,
    2,  29, 2, 2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2, 2, 2,  2,
    2,  2,  2, 2,  2,  2,  2,  2,  2,  2,  2,  27, 2,  28, 2,  2,  2, 2, 2,  2,
    2,  2,  2, 2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2, 2, 2,  2,
    2,  2,  2, 30, 2,  31, 2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2, 2, 2,  2,
    2,  2,  2, 2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2, 2, 2,  2,
    2,  2,  2, 2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2, 2, 2,  2,
    2,  2,  2, 2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2, 2, 2,  2,
    2,  2,  2, 2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2, 2, 2,  2,
    2,  2,  2, 2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2, 2, 2,  2,
    2,  2,  2, 2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  2,  1, 2, 3,  4,
    5,  6,  7, 8,  9,  10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21};

#if ORFZ_DEBUG
/* YYRLINE[YYN] -- Source line where rule number YYN was defined.  */
static const yytype_int16 yyrline[] = {
    0,   105, 105, 112, 116, 117, 122, 125, 126, 129, 130, 131, 132, 135, 136,
    139, 140, 147, 148, 151, 170, 185, 196, 211, 222, 248, 298, 383, 384, 387,
    388, 389, 392, 396, 402, 403, 404, 407, 412, 427, 448, 449, 450, 451, 458,
    459, 460, 461, 468, 469, 474, 475, 476, 479, 480, 483, 484, 485, 490, 491,
    494, 495, 496, 501, 502, 503, 508, 509, 512, 513, 519, 523, 529, 530, 533,
    545, 546, 549, 550, 551, 552, 553, 558, 584, 601, 649, 658, 662, 665, 666,
    669, 670, 671, 672, 686, 695, 701, 730, 739, 747, 758};
#endif

/** Accessing symbol of state STATE.  */
#define YY_ACCESSING_SYMBOL(State) YY_CAST(yysymbol_kind_t, yystos[State])

#if 1
/* The user-facing name of the symbol whose (internal) number is
   YYSYMBOL.  No bounds checking.  */
static const char* yysymbol_name(yysymbol_kind_t yysymbol) YY_ATTRIBUTE_UNUSED;

/* YYTNAME[SYMBOL-NUM] -- String name of the symbol SYMBOL-NUM.
   First, the terminals, then, starting at YYNTOKENS, nonterminals.  */
static const char* const yytname[] = {"\"end of file\"",
                                      "error",
                                      "\"invalid token\"",
                                      "ARRAY",
                                      "TOKEN_BOOL",
                                      "CONSTRAINT",
                                      "TOKEN_FLOAT",
                                      "TOKEN_INT",
                                      "MAXIMIZE",
                                      "MINIMIZE",
                                      "OF",
                                      "PREDICATE",
                                      "SATISFY",
                                      "SET",
                                      "SOLVE",
                                      "VAR",
                                      "DOTDOT",
                                      "COLONCOLON",
                                      "IVALUE",
                                      "SVALUE",
                                      "IDENTIFIER",
                                      "DVALUE",
                                      "';'",
                                      "'('",
                                      "')'",
                                      "','",
                                      "':'",
                                      "'['",
                                      "']'",
                                      "'='",
                                      "'{'",
                                      "'}'",
                                      "$accept",
                                      "model",
                                      "predicates",
                                      "predicate",
                                      "predicate_arguments",
                                      "predicate_argument",
                                      "predicate_array_argument",
                                      "predicate_ints",
                                      "variable_or_constant_declarations",
                                      "variable_or_constant_declaration",
                                      "optional_var_or_value",
                                      "optional_var_or_value_array",
                                      "var_or_value_array",
                                      "var_or_value",
                                      "int_domain",
                                      "set_domain",
                                      "float_domain",
                                      "domain",
                                      "integers",
                                      "integer",
                                      "floats",
                                      "float",
                                      "const_literal",
                                      "const_literals",
                                      "constraints",
                                      "constraint",
                                      "arguments",
                                      "argument",
                                      "annotations",
                                      "annotation_arguments",
                                      "annotation",
                                      "solve",
                                      YY_NULLPTR};

static const char* yysymbol_name(yysymbol_kind_t yysymbol) {
  return yytname[yysymbol];
}
#endif

#define YYPACT_NINF (-190)

#define yypact_value_is_default(Yyn) ((Yyn) == YYPACT_NINF)

#define YYTABLE_NINF (-19)

#define yytable_value_is_error(Yyn) 0

/* YYPACT[STATE-NUM] -- Index in YYTABLE of the portion describing
   STATE-NUM.  */
static const yytype_int16 yypact[] = {
    -190, 34,   7,    -190, -15,  82,   72,   20,   -190, 84,   -190, 103,
    -190, -190, -190, 135,  75,   105,  151,  42,   153,  -190, -190, -190,
    164,  112,  36,   163,  83,   165,  174,  172,  -190, 167,  134,  -190,
    -190, 175,  176,  -190, 177,  178,  170,  75,   179,  173,  180,  185,
    -190, -190, 186,  42,   184,  -190, -190, 187,  42,   -190, -190, 188,
    141,  -190, -190, 49,   181,  -190, 36,   189,  190,  192,  139,  -190,
    191,  -190, 35,   79,   79,   79,   -190, 142,  193,  196,  194,  -190,
    195,  -190, -190, 197,  -190, -190, 46,   -190, 107,  198,  -190, 199,
    -190, 90,   42,   160,  -190, -190, -190, 200,  -190, 96,   124,  -190,
    206,  202,  207,  -190, 211,  111,  -190, 208,  201,  -190, -1,   -190,
    205,  209,  213,  203,  -190, -190, 42,   4,    -190, 143,  -190, 79,
    214,  142,  215,  -190, 64,   -190, -190, -190, 40,   65,   -190, 216,
    217,  -190, 146,  -190, 210,  218,  219,  147,  111,  -190, -190, 222,
    -190, -190, 162,  212,  142,  -190, 75,   220,  75,   221,  223,  224,
    -190, 225,  -190, -190, -190, 226,  -190, -190, -190, -190, -190, 229,
    228,  230,  231,  232,  237,  -190, -190, 238,  -190, 239,  -190, -190,
    -190, -190, -190, 48,   56,   74,   87,   204,  233,  234,  235,  -190,
    128,  107,  119,  94,   -190, 138,  -190, 148,  236,  -190, -190, 154,
    -190, -190, 155,  -190, 107,  -190, 227,  168,  -190, -190, -190, 240,
    -190, -190};

/* YYDEFACT[STATE-NUM] -- Default reduction number in state STATE-NUM.
   Performed when YYTABLE does not specify something else to do.  Zero
   means the default is an error.  */
static const yytype_int8 yydefact[] = {
    5,  0,  0,  1,  0,  0,  0,  73, 4,  0,  3,  0,   40, 48, 41, 0,  0,  0,
    0,  0,  0,  50, 51, 52, 0,  0,  0,  0,  0,  0,   0,  0,  55, 56, 0,  54,
    17, 0,  0,  87, 0,  0,  0,  0,  0,  8,  0,  0,   44, 45, 0,  0,  0,  42,
    49, 0,  0,  43, 87, 0,  0,  72, 2,  0,  0,  6,   0,  0,  0,  0,  0,  87,
    0,  53, 0,  0,  0,  0,  98, 0,  16, 0,  0,  13,  0,  7,  9,  0,  46, 47,
    28, 57, 0,  77, 79, 82, 78, 0,  0,  0,  76, 100, 99, 91, 92, 93, 0,  86,
    0,  0,  0,  10, 0,  0,  25, 63, 68, 67, 0,  19,  0,  0,  34, 38, 35, 85,
    0,  0,  33, 0,  87, 0,  0,  0,  0,  97, 0,  89,  15, 14, 0,  0,  27, 0,
    0,  66, 0,  80, 0,  0,  0,  0,  0,  84, 81, 74,  75, 90, 0,  0,  0,  96,
    0,  0,  0,  0,  0,  0,  64, 0,  65, 83, 36, 0,   37, 32, 94, 95, 88, 0,
    0,  0,  0,  0,  0,  69, 39, 0,  11, 0,  87, 87,  87, 12, 87, 0,  0,  0,
    31, 0,  0,  0,  0,  26, 0,  0,  0,  0,  21, 0,   71, 0,  61, 60, 23, 0,
    59, 30, 0,  20, 0,  24, 0,  0,  22, 29, 70, 0,   58, 62};

/* YYPGOTO[NTERM-NUM].  */
static const yytype_int16 yypgoto[] = {
    -190, -190, -190, -190, 241,  -190, -190, 133, -190, -190, -190,
    -190, 37,   -107, 101,  102,  123,  -7,   -50, 242,  -190, 43,
    -189, -190, -190, -190, -190, -72,  -56,  132, -76,  -190};

/* YYDEFGOTO[NTERM-NUM].  */
static const yytype_uint8 yydefgoto[] = {
    0,  1,  2,  6,  44,  45,  82,  83,  7,  20, 114, 203, 127, 128, 21,  22,
    23, 46, 34, 35, 215, 216, 119, 211, 25, 40, 99,  100, 60,  136, 137, 41};

/* YYTABLE[YYPACT[STATE-NUM]] -- What to do in state STATE-NUM.  If
   positive, shift that token.  If negative, reduce the rule whose
   number is the opposite.  If YYTABLE_NINF, syntax error.  */
static const yytype_int16 yytable[] = {
    24,  70,  74,  107, 101, 102, 142, 8,   4,   29,  -18, -18, -18, -18, -18,
    90,  210, 32,  5,   33,  -18, -18, -18, 11,  12,  -18, 13,  14,  -18, 152,
    145, 226, 153, 15,  3,   16,  64,  -18, 17,  42,  12,  18,  13,  14,  12,
    175, 13,  14,  129, 15,  19,  43,  79,  15,  17,  162, 80,  18,  17,  156,
    32,  18,  33,  79,  92,  79,  19,  81,  146, 12,  19,  13,  14,  79,  155,
    113, 151, 199, 15,  12,  164, 13,  14,  17,  178, 200, 18,  48,  15,  160,
    49,  79,  161, 17,  10,  19,  18,  93,  94,  95,  96,  50,  9,   201, 79,
    19,  97,  26,  122, 98,  123, 124, 122, 51,  123, 124, 202, 38,  125, 133,
    126, 30,  217, 134, 126, 115, 39,  116, 117, 122, 27,  123, 124, 163, 195,
    196, 197, 118, 198, 212, 213, 126, 103, 104, 105, 28,  32,  214, 33,  76,
    77,  106, 135, 78,  209, 179, 208, 181, 79,  56,  103, 104, 105, 56,  56,
    57,  219, 31,  56,  106, 89,  56,  56,  220, 154, 36,  221, 170, 174, 223,
    152, 47,  224, 225, 130, 131, 176, 160, 212, 213, 37,  52,  53,  54,  55,
    58,  59,  63,  66,  61,  62,  68,  69,  65,  71,  72,  67,  84,  87,  86,
    88,  75,  109, 80,  120, 111, 132, 140, 108, 91,  139, 141, 110, 147, 143,
    112, 121, 148, 144, 149, 150, 204, 157, 159, 168, 169, 172, 173, 171, 79,
    177, 138, 165, 166, 218, 227, 180, 182, 188, 183, 184, 190, 191, 185, 186,
    187, 189, 192, 193, 194, 205, 206, 207, 222, 167, 158, 228, 0,   229, 0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   73,  0,
    0,   0,   0,   0,   0,   0,   0,   85};

static const yytype_int16 yycheck[] = {
    7,   51,  58,  79,  76,  77,  113, 22,  1,   16,  3,   4,   5,  6,   7,
    71,  205, 18,  11,  20,  13,  14,  15,  3,   4,   18,  6,   7,  21,  25,
    31,  220, 28,  13,  0,   15,  43,  30,  18,  3,   4,   21,  6,  7,   4,
    152, 6,   7,   98,  13,  30,  15,  17,  13,  18,  15,  7,   21, 18,  131,
    18,  21,  20,  17,  29,  17,  30,  18,  118, 4,   30,  6,   7,  17,  130,
    29,  126, 29,  13,  4,   15,  6,   7,   18,  160, 29,  21,  4,  13,  25,
    7,   17,  28,  18,  22,  30,  21,  18,  19,  20,  21,  18,  20, 29,  17,
    30,  27,  23,  18,  30,  20,  21,  18,  30,  20,  21,  29,  5,  28,  23,
    30,  16,  28,  27,  30,  18,  14,  20,  21,  18,  27,  20,  21, 140, 190,
    191, 192, 30,  194, 20,  21,  30,  18,  19,  20,  10,  18,  28, 20,  8,
    9,   27,  28,  12,  204, 162, 28,  164, 17,  25,  18,  19,  20, 25,  25,
    31,  28,  16,  25,  27,  31,  25,  25,  25,  31,  22,  28,  31, 31,  25,
    25,  18,  28,  28,  24,  25,  24,  25,  20,  21,  26,  26,  18, 21,  27,
    20,  20,  27,  25,  22,  22,  16,  16,  24,  20,  18,  26,  26, 18,  20,
    18,  23,  16,  7,   16,  20,  16,  10,  25,  28,  18,  10,  28, 18,  16,
    28,  27,  18,  27,  16,  27,  27,  18,  18,  18,  18,  18,  18, 28,  17,
    28,  108, 141, 141, 207, 18,  26,  26,  20,  26,  26,  20,  20, 28,  28,
    26,  26,  20,  20,  20,  27,  27,  27,  27,  141, 133, 223, -1, 28,  -1,
    -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1, -1,  -1,
    -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1,  -1, 56,  -1,
    -1,  -1,  -1,  -1,  -1,  -1,  -1,  66};

/* YYSTOS[STATE-NUM] -- The symbol kind of the accessing symbol of
   state STATE-NUM.  */
static const yytype_int8 yystos[] = {
    0,  33, 34, 0,  1,  11, 35, 40, 22, 20, 22, 3,  4,  6,  7,  13, 15, 18,
    21, 30, 41, 46, 47, 48, 49, 56, 23, 27, 10, 49, 16, 16, 18, 20, 50, 51,
    22, 26, 5,  14, 57, 63, 3,  15, 36, 37, 49, 18, 4,  7,  18, 30, 26, 18,
    21, 27, 25, 31, 20, 20, 60, 22, 22, 27, 49, 24, 25, 26, 16, 16, 50, 20,
    18, 51, 60, 23, 8,  9,  12, 17, 7,  18, 38, 39, 26, 36, 20, 18, 18, 31,
    60, 28, 29, 18, 19, 20, 21, 27, 30, 58, 59, 59, 59, 18, 19, 20, 27, 62,
    25, 16, 28, 20, 28, 29, 42, 18, 20, 21, 30, 54, 16, 27, 18, 20, 21, 28,
    30, 44, 45, 50, 24, 25, 16, 23, 27, 28, 61, 62, 39, 18, 10, 10, 45, 16,
    27, 31, 50, 18, 18, 16, 27, 50, 25, 28, 31, 60, 59, 18, 61, 18, 25, 28,
    15, 49, 15, 46, 47, 48, 18, 18, 31, 28, 18, 18, 31, 45, 24, 28, 62, 49,
    26, 49, 26, 26, 26, 28, 28, 26, 20, 26, 20, 20, 20, 20, 20, 60, 60, 60,
    60, 29, 29, 29, 29, 43, 27, 27, 27, 27, 28, 50, 54, 55, 20, 21, 28, 52,
    53, 28, 44, 28, 25, 28, 27, 25, 28, 28, 54, 18, 53, 28};

/* YYR1[RULE-NUM] -- Symbol kind of the left-hand side of rule RULE-NUM.  */
static const yytype_int8 yyr1[] = {
    0,  32, 33, 34, 34, 34, 35, 36, 36, 37, 37, 37, 37, 38, 38, 39, 39,
    40, 40, 41, 41, 41, 41, 41, 41, 41, 41, 42, 42, 43, 43, 43, 44, 44,
    45, 45, 45, 45, 45, 45, 46, 46, 46, 46, 47, 47, 47, 47, 48, 48, 49,
    49, 49, 50, 50, 51, 51, 51, 52, 52, 53, 53, 53, 54, 54, 54, 54, 54,
    54, 54, 55, 55, 56, 56, 57, 58, 58, 59, 59, 59, 59, 59, 59, 59, 59,
    59, 60, 60, 61, 61, 62, 62, 62, 62, 62, 62, 62, 62, 63, 63, 63};

/* YYR2[RULE-NUM] -- Number of symbols on the right-hand side of rule RULE-NUM.
 */
static const yytype_int8 yyr2[] = {
    0,  2,  5,  3,  3, 0,  5, 3, 1, 3, 4, 8, 9, 1, 3, 3, 1, 3, 0, 6, 15,
    14, 15, 14, 15, 6, 13, 2, 0, 4, 3, 0, 3, 1, 1, 1, 3, 3, 1, 4, 1, 1,
    3,  3,  3,  3,  5, 5,  1, 3, 1, 1, 1, 3, 1, 1, 1, 4, 3, 1, 1, 1, 4,
    1,  3,  3,  2,  1, 1,  4, 3, 1, 3, 0, 6, 3, 1, 1, 1, 1, 3, 3, 1, 4,
    3,  2,  3,  0,  3, 1,  3, 1, 1, 1, 4, 4, 3, 2, 3, 4, 4};

enum { YYENOMEM = -2 };

#define yyerrok (yyerrstatus = 0)
#define yyclearin (yychar = ORFZ_EMPTY)

#define YYACCEPT goto yyacceptlab
#define YYABORT goto yyabortlab
#define YYERROR goto yyerrorlab
#define YYNOMEM goto yyexhaustedlab

#define YYRECOVERING() (!!yyerrstatus)

#define YYBACKUP(Token, Value)                      \
  do                                                \
    if (yychar == ORFZ_EMPTY) {                     \
      yychar = (Token);                             \
      yylval = (Value);                             \
      YYPOPSTACK(yylen);                            \
      yystate = *yyssp;                             \
      goto yybackup;                                \
    } else {                                        \
      yyerror(context, model, ok, scanner,          \
              YY_("syntax error: cannot back up")); \
      YYERROR;                                      \
    }                                               \
  while (0)

/* Backward compatibility with an undocumented macro.
   Use ORFZ_error or ORFZ_UNDEF. */
#define YYERRCODE ORFZ_UNDEF

/* Enable debugging if requested.  */
#if ORFZ_DEBUG

#ifndef YYFPRINTF
#include <stdio.h> /* INFRINGES ON USER NAME SPACE */
#define YYFPRINTF fprintf
#endif

#define YYDPRINTF(Args)          \
  do {                           \
    if (yydebug) YYFPRINTF Args; \
  } while (0)

#define YY_SYMBOL_PRINT(Title, Kind, Value, Location)                    \
  do {                                                                   \
    if (yydebug) {                                                       \
      YYFPRINTF(stderr, "%s ", Title);                                   \
      yy_symbol_print(stderr, Kind, Value, context, model, ok, scanner); \
      YYFPRINTF(stderr, "\n");                                           \
    }                                                                    \
  } while (0)

/*-----------------------------------.
| Print this symbol's value on YYO.  |
`-----------------------------------*/

static void yy_symbol_value_print(
    FILE* yyo, yysymbol_kind_t yykind, YYSTYPE const* const yyvaluep,
    operations_research::fz::ParserContext* context,
    operations_research::fz::Model* model, bool* ok, void* scanner) {
  FILE* yyoutput = yyo;
  YY_USE(yyoutput);
  YY_USE(context);
  YY_USE(model);
  YY_USE(ok);
  YY_USE(scanner);
  if (!yyvaluep) return;
  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  YY_USE(yykind);
  YY_IGNORE_MAYBE_UNINITIALIZED_END
}

/*---------------------------.
| Print this symbol on YYO.  |
`---------------------------*/

static void yy_symbol_print(FILE* yyo, yysymbol_kind_t yykind,
                            YYSTYPE const* const yyvaluep,
                            operations_research::fz::ParserContext* context,
                            operations_research::fz::Model* model, bool* ok,
                            void* scanner) {
  YYFPRINTF(yyo, "%s %s (", yykind < YYNTOKENS ? "token" : "nterm",
            yysymbol_name(yykind));

  yy_symbol_value_print(yyo, yykind, yyvaluep, context, model, ok, scanner);
  YYFPRINTF(yyo, ")");
}

/*------------------------------------------------------------------.
| yy_stack_print -- Print the state stack from its BOTTOM up to its |
| TOP (included).                                                   |
`------------------------------------------------------------------*/

static void yy_stack_print(yy_state_t* yybottom, yy_state_t* yytop) {
  YYFPRINTF(stderr, "Stack now");
  for (; yybottom <= yytop; yybottom++) {
    int yybot = *yybottom;
    YYFPRINTF(stderr, " %d", yybot);
  }
  YYFPRINTF(stderr, "\n");
}

#define YY_STACK_PRINT(Bottom, Top)               \
  do {                                            \
    if (yydebug) yy_stack_print((Bottom), (Top)); \
  } while (0)

/*------------------------------------------------.
| Report that the YYRULE is going to be reduced.  |
`------------------------------------------------*/

static void yy_reduce_print(yy_state_t* yyssp, YYSTYPE* yyvsp, int yyrule,
                            operations_research::fz::ParserContext* context,
                            operations_research::fz::Model* model, bool* ok,
                            void* scanner) {
  int yylno = yyrline[yyrule];
  int yynrhs = yyr2[yyrule];
  int yyi;
  YYFPRINTF(stderr, "Reducing stack by rule %d (line %d):\n", yyrule - 1,
            yylno);
  /* The symbols being reduced.  */
  for (yyi = 0; yyi < yynrhs; yyi++) {
    YYFPRINTF(stderr, "   $%d = ", yyi + 1);
    yy_symbol_print(stderr, YY_ACCESSING_SYMBOL(+yyssp[yyi + 1 - yynrhs]),
                    &yyvsp[(yyi + 1) - (yynrhs)], context, model, ok, scanner);
    YYFPRINTF(stderr, "\n");
  }
}

#define YY_REDUCE_PRINT(Rule)                                           \
  do {                                                                  \
    if (yydebug)                                                        \
      yy_reduce_print(yyssp, yyvsp, Rule, context, model, ok, scanner); \
  } while (0)

/* Nonzero means print parse trace.  It is left uninitialized so that
   multiple parsers can coexist.  */
int yydebug;
#else /* !ORFZ_DEBUG */
#define YYDPRINTF(Args) ((void)0)
#define YY_SYMBOL_PRINT(Title, Kind, Value, Location)
#define YY_STACK_PRINT(Bottom, Top)
#define YY_REDUCE_PRINT(Rule)
#endif /* !ORFZ_DEBUG */

/* YYINITDEPTH -- initial size of the parser's stacks.  */
#ifndef YYINITDEPTH
#define YYINITDEPTH 200
#endif

/* YYMAXDEPTH -- maximum size the stacks can grow to (effective only
   if the built-in stack extension method is used).

   Do not make this value too large; the results are undefined if
   YYSTACK_ALLOC_MAXIMUM < YYSTACK_BYTES (YYMAXDEPTH)
   evaluated with infinite-precision integer arithmetic.  */

#ifndef YYMAXDEPTH
#define YYMAXDEPTH 10000
#endif

/* Context of a parse error.  */
typedef struct {
  yy_state_t* yyssp;
  yysymbol_kind_t yytoken;
} yypcontext_t;

/* Put in YYARG at most YYARGN of the expected tokens given the
   current YYCTX, and return the number of tokens stored in YYARG.  If
   YYARG is null, return the number of expected tokens (guaranteed to
   be less than YYNTOKENS).  Return YYENOMEM on memory exhaustion.
   Return 0 if there are more than YYARGN expected tokens, yet fill
   YYARG up to YYARGN. */
static int yypcontext_expected_tokens(const yypcontext_t* yyctx,
                                      yysymbol_kind_t yyarg[], int yyargn) {
  /* Actual size of YYARG. */
  int yycount = 0;
  int yyn = yypact[+*yyctx->yyssp];
  if (!yypact_value_is_default(yyn)) {
    /* Start YYX at -YYN if negative to avoid negative indexes in
       YYCHECK.  In other words, skip the first -YYN actions for
       this state because they are default actions.  */
    int yyxbegin = yyn < 0 ? -yyn : 0;
    /* Stay within bounds of both yycheck and yytname.  */
    int yychecklim = YYLAST - yyn + 1;
    int yyxend = yychecklim < YYNTOKENS ? yychecklim : YYNTOKENS;
    int yyx;
    for (yyx = yyxbegin; yyx < yyxend; ++yyx)
      if (yycheck[yyx + yyn] == yyx && yyx != YYSYMBOL_YYerror &&
          !yytable_value_is_error(yytable[yyx + yyn])) {
        if (!yyarg)
          ++yycount;
        else if (yycount == yyargn)
          return 0;
        else
          yyarg[yycount++] = YY_CAST(yysymbol_kind_t, yyx);
      }
  }
  if (yyarg && yycount == 0 && 0 < yyargn) yyarg[0] = YYSYMBOL_YYEMPTY;
  return yycount;
}

#ifndef yystrlen
#if defined __GLIBC__ && defined _STRING_H
#define yystrlen(S) (YY_CAST(YYPTRDIFF_T, strlen(S)))
#else
/* Return the length of YYSTR.  */
static YYPTRDIFF_T yystrlen(const char* yystr) {
  YYPTRDIFF_T yylen;
  for (yylen = 0; yystr[yylen]; yylen++) continue;
  return yylen;
}
#endif
#endif

#ifndef yystpcpy
#if defined __GLIBC__ && defined _STRING_H && defined _GNU_SOURCE
#define yystpcpy stpcpy
#else
/* Copy YYSRC to YYDEST, returning the address of the terminating '\0' in
   YYDEST.  */
static char* yystpcpy(char* yydest, const char* yysrc) {
  char* yyd = yydest;
  const char* yys = yysrc;

  while ((*yyd++ = *yys++) != '\0') continue;

  return yyd - 1;
}
#endif
#endif

#ifndef yytnamerr
/* Copy to YYRES the contents of YYSTR after stripping away unnecessary
   quotes and backslashes, so that it's suitable for yyerror.  The
   heuristic is that double-quoting is unnecessary unless the string
   contains an apostrophe, a comma, or backslash (other than
   backslash-backslash).  YYSTR is taken from yytname.  If YYRES is
   null, do not copy; instead, return the length of what the result
   would have been.  */
static YYPTRDIFF_T yytnamerr(char* yyres, const char* yystr) {
  if (*yystr == '"') {
    YYPTRDIFF_T yyn = 0;
    char const* yyp = yystr;
    for (;;) switch (*++yyp) {
        case '\'':
        case ',':
          goto do_not_strip_quotes;

        case '\\':
          if (*++yyp != '\\')
            goto do_not_strip_quotes;
          else
            goto append;

        append:
        default:
          if (yyres) yyres[yyn] = *yyp;
          yyn++;
          break;

        case '"':
          if (yyres) yyres[yyn] = '\0';
          return yyn;
      }
  do_not_strip_quotes:;
  }

  if (yyres)
    return yystpcpy(yyres, yystr) - yyres;
  else
    return yystrlen(yystr);
}
#endif

static int yy_syntax_error_arguments(const yypcontext_t* yyctx,
                                     yysymbol_kind_t yyarg[], int yyargn) {
  /* Actual size of YYARG. */
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
  if (yyctx->yytoken != YYSYMBOL_YYEMPTY) {
    int yyn;
    if (yyarg) yyarg[yycount] = yyctx->yytoken;
    ++yycount;
    yyn = yypcontext_expected_tokens(yyctx, yyarg ? yyarg + 1 : yyarg,
                                     yyargn - 1);
    if (yyn == YYENOMEM)
      return YYENOMEM;
    else
      yycount += yyn;
  }
  return yycount;
}

/* Copy into *YYMSG, which is of size *YYMSG_ALLOC, an error message
   about the unexpected token YYTOKEN for the state stack whose top is
   YYSSP.

   Return 0 if *YYMSG was successfully written.  Return -1 if *YYMSG is
   not large enough to hold the message.  In that case, also set
   *YYMSG_ALLOC to the required number of bytes.  Return YYENOMEM if the
   required number of bytes is too large to store.  */
static int yysyntax_error(YYPTRDIFF_T* yymsg_alloc, char** yymsg,
                          const yypcontext_t* yyctx) {
  enum { YYARGS_MAX = 5 };
  /* Internationalized format string. */
  const char* yyformat = YY_NULLPTR;
  /* Arguments of yyformat: reported tokens (one for the "unexpected",
     one per "expected"). */
  yysymbol_kind_t yyarg[YYARGS_MAX];
  /* Cumulated lengths of YYARG.  */
  YYPTRDIFF_T yysize = 0;

  /* Actual size of YYARG. */
  int yycount = yy_syntax_error_arguments(yyctx, yyarg, YYARGS_MAX);
  if (yycount == YYENOMEM) return YYENOMEM;

  switch (yycount) {
#define YYCASE_(N, S) \
  case N:             \
    yyformat = S;     \
    break
    default: /* Avoid compiler warnings. */
      YYCASE_(0, YY_("syntax error"));
      YYCASE_(1, YY_("syntax error, unexpected %s"));
      YYCASE_(2, YY_("syntax error, unexpected %s, expecting %s"));
      YYCASE_(3, YY_("syntax error, unexpected %s, expecting %s or %s"));
      YYCASE_(4, YY_("syntax error, unexpected %s, expecting %s or %s or %s"));
      YYCASE_(
          5,
          YY_("syntax error, unexpected %s, expecting %s or %s or %s or %s"));
#undef YYCASE_
  }

  /* Compute error message size.  Don't count the "%s"s, but reserve
     room for the terminator.  */
  yysize = yystrlen(yyformat) - 2 * yycount + 1;
  {
    int yyi;
    for (yyi = 0; yyi < yycount; ++yyi) {
      YYPTRDIFF_T yysize1 = yysize + yytnamerr(YY_NULLPTR, yytname[yyarg[yyi]]);
      if (yysize <= yysize1 && yysize1 <= YYSTACK_ALLOC_MAXIMUM)
        yysize = yysize1;
      else
        return YYENOMEM;
    }
  }

  if (*yymsg_alloc < yysize) {
    *yymsg_alloc = 2 * yysize;
    if (!(yysize <= *yymsg_alloc && *yymsg_alloc <= YYSTACK_ALLOC_MAXIMUM))
      *yymsg_alloc = YYSTACK_ALLOC_MAXIMUM;
    return -1;
  }

  /* Avoid sprintf, as that infringes on the user's name space.
     Don't have undefined behavior even if the translation
     produced a string with the wrong number of "%s"s.  */
  {
    char* yyp = *yymsg;
    int yyi = 0;
    while ((*yyp = *yyformat) != '\0')
      if (*yyp == '%' && yyformat[1] == 's' && yyi < yycount) {
        yyp += yytnamerr(yyp, yytname[yyarg[yyi++]]);
        yyformat += 2;
      } else {
        ++yyp;
        ++yyformat;
      }
  }
  return 0;
}

/*-----------------------------------------------.
| Release the memory associated to this symbol.  |
`-----------------------------------------------*/

static void yydestruct(const char* yymsg, yysymbol_kind_t yykind,
                       YYSTYPE* yyvaluep,
                       operations_research::fz::ParserContext* context,
                       operations_research::fz::Model* model, bool* ok,
                       void* scanner) {
  YY_USE(yyvaluep);
  YY_USE(context);
  YY_USE(model);
  YY_USE(ok);
  YY_USE(scanner);
  if (!yymsg) yymsg = "Deleting";
  YY_SYMBOL_PRINT(yymsg, yykind, yyvaluep, yylocationp);

  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  YY_USE(yykind);
  YY_IGNORE_MAYBE_UNINITIALIZED_END
}

/*----------.
| yyparse.  |
`----------*/

int yyparse(operations_research::fz::ParserContext* context,
            operations_research::fz::Model* model, bool* ok, void* scanner) {
  /* Lookahead token kind.  */
  int yychar;

  /* The semantic value of the lookahead symbol.  */
  /* Default value used for initialization, for pacifying older GCCs
     or non-GCC compilers.  */
  YY_INITIAL_VALUE(static YYSTYPE yyval_default;)
  YYSTYPE yylval YY_INITIAL_VALUE(= yyval_default);

  /* Number of syntax errors so far.  */
  int yynerrs = 0;

  yy_state_fast_t yystate = 0;
  /* Number of tokens to shift before error messages enabled.  */
  int yyerrstatus = 0;

  /* Refer to the stacks through separate pointers, to allow yyoverflow
     to reallocate them elsewhere.  */

  /* Their size.  */
  YYPTRDIFF_T yystacksize = YYINITDEPTH;

  /* The state stack: array, bottom, top.  */
  yy_state_t yyssa[YYINITDEPTH];
  yy_state_t* yyss = yyssa;
  yy_state_t* yyssp = yyss;

  /* The semantic value stack: array, bottom, top.  */
  YYSTYPE yyvsa[YYINITDEPTH];
  YYSTYPE* yyvs = yyvsa;
  YYSTYPE* yyvsp = yyvs;

  int yyn;
  /* The return value of yyparse.  */
  int yyresult;
  /* Lookahead symbol kind.  */
  yysymbol_kind_t yytoken = YYSYMBOL_YYEMPTY;
  /* The variables used to return semantic value and location from the
     action routines.  */
  YYSTYPE yyval;

  /* Buffer for error messages, and its allocated size.  */
  char yymsgbuf[128];
  char* yymsg = yymsgbuf;
  YYPTRDIFF_T yymsg_alloc = sizeof yymsgbuf;

#define YYPOPSTACK(N) (yyvsp -= (N), yyssp -= (N))

  /* The number of symbols on the RHS of the reduced rule.
     Keep to zero when no symbol should be popped.  */
  int yylen = 0;

  YYDPRINTF((stderr, "Starting parse\n"));

  yychar = ORFZ_EMPTY; /* Cause a token to be read.  */

  goto yysetstate;

/*------------------------------------------------------------.
| yynewstate -- push a new state, which is found in yystate.  |
`------------------------------------------------------------*/
yynewstate:
  /* In all cases, when you get here, the value and location stacks
     have just been pushed.  So pushing a state here evens the stacks.  */
  yyssp++;

/*--------------------------------------------------------------------.
| yysetstate -- set current state (the top of the stack) to yystate.  |
`--------------------------------------------------------------------*/
yysetstate:
  YYDPRINTF((stderr, "Entering state %d\n", yystate));
  YY_ASSERT(0 <= yystate && yystate < YYNSTATES);
  YY_IGNORE_USELESS_CAST_BEGIN
  *yyssp = YY_CAST(yy_state_t, yystate);
  YY_IGNORE_USELESS_CAST_END
  YY_STACK_PRINT(yyss, yyssp);

  if (yyss + yystacksize - 1 <= yyssp)
#if !defined yyoverflow && !defined YYSTACK_RELOCATE
    YYNOMEM;
#else
  {
    /* Get the current used size of the three stacks, in elements.  */
    YYPTRDIFF_T yysize = yyssp - yyss + 1;

#if defined yyoverflow
    {
      /* Give user a chance to reallocate the stack.  Use copies of
         these so that the &'s don't force the real ones into
         memory.  */
      yy_state_t* yyss1 = yyss;
      YYSTYPE* yyvs1 = yyvs;

      /* Each stack pointer address is followed by the size of the
         data in use in that stack, in bytes.  This used to be a
         conditional around just the two extra args, but that might
         be undefined if yyoverflow is a macro.  */
      yyoverflow(YY_("memory exhausted"), &yyss1, yysize * YYSIZEOF(*yyssp),
                 &yyvs1, yysize * YYSIZEOF(*yyvsp), &yystacksize);
      yyss = yyss1;
      yyvs = yyvs1;
    }
#else /* defined YYSTACK_RELOCATE */
    /* Extend the stack our own way.  */
    if (YYMAXDEPTH <= yystacksize) YYNOMEM;
    yystacksize *= 2;
    if (YYMAXDEPTH < yystacksize) yystacksize = YYMAXDEPTH;

    {
      yy_state_t* yyss1 = yyss;
      union yyalloc* yyptr =
          YY_CAST(union yyalloc*,
                  YYSTACK_ALLOC(YY_CAST(YYSIZE_T, YYSTACK_BYTES(yystacksize))));
      if (!yyptr) YYNOMEM;
      YYSTACK_RELOCATE(yyss_alloc, yyss);
      YYSTACK_RELOCATE(yyvs_alloc, yyvs);
#undef YYSTACK_RELOCATE
      if (yyss1 != yyssa) YYSTACK_FREE(yyss1);
    }
#endif

    yyssp = yyss + yysize - 1;
    yyvsp = yyvs + yysize - 1;

    YY_IGNORE_USELESS_CAST_BEGIN
    YYDPRINTF(
        (stderr, "Stack size increased to %ld\n", YY_CAST(long, yystacksize)));
    YY_IGNORE_USELESS_CAST_END

    if (yyss + yystacksize - 1 <= yyssp) YYABORT;
  }
#endif /* !defined yyoverflow && !defined YYSTACK_RELOCATE */

  if (yystate == YYFINAL) YYACCEPT;

  goto yybackup;

/*-----------.
| yybackup.  |
`-----------*/
yybackup:
  /* Do appropriate processing given the current state.  Read a
     lookahead token if we need one and don't already have one.  */

  /* First try to decide what to do without reference to lookahead token.  */
  yyn = yypact[yystate];
  if (yypact_value_is_default(yyn)) goto yydefault;

  /* Not known => get a lookahead token if don't already have one.  */

  /* YYCHAR is either empty, or end-of-input, or a valid lookahead.  */
  if (yychar == ORFZ_EMPTY) {
    YYDPRINTF((stderr, "Reading a token\n"));
    yychar = yylex(&yylval, scanner);
  }

  if (yychar <= ORFZ_EOF) {
    yychar = ORFZ_EOF;
    yytoken = YYSYMBOL_YYEOF;
    YYDPRINTF((stderr, "Now at end of input.\n"));
  } else if (yychar == ORFZ_error) {
    /* The scanner already issued an error message, process directly
       to error recovery.  But do not keep the error token as
       lookahead, it is too special and may lead us to an endless
       loop in error recovery. */
    yychar = ORFZ_UNDEF;
    yytoken = YYSYMBOL_YYerror;
    goto yyerrlab1;
  } else {
    yytoken = YYTRANSLATE(yychar);
    YY_SYMBOL_PRINT("Next token is", yytoken, &yylval, &yylloc);
  }

  /* If the proper action on seeing token YYTOKEN is to reduce or to
     detect an error, take that action.  */
  yyn += yytoken;
  if (yyn < 0 || YYLAST < yyn || yycheck[yyn] != yytoken) goto yydefault;
  yyn = yytable[yyn];
  if (yyn <= 0) {
    if (yytable_value_is_error(yyn)) goto yyerrlab;
    yyn = -yyn;
    goto yyreduce;
  }

  /* Count tokens shifted since error; after three, turn off error
     status.  */
  if (yyerrstatus) yyerrstatus--;

  /* Shift the lookahead token.  */
  YY_SYMBOL_PRINT("Shifting", yytoken, &yylval, &yylloc);
  yystate = yyn;
  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  *++yyvsp = yylval;
  YY_IGNORE_MAYBE_UNINITIALIZED_END

  /* Discard the shifted token.  */
  yychar = ORFZ_EMPTY;
  goto yynewstate;

/*-----------------------------------------------------------.
| yydefault -- do the default action for the current state.  |
`-----------------------------------------------------------*/
yydefault:
  yyn = yydefact[yystate];
  if (yyn == 0) goto yyerrlab;
  goto yyreduce;

/*-----------------------------.
| yyreduce -- do a reduction.  |
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
  yyval = yyvsp[1 - yylen];

  YY_REDUCE_PRINT(yyn);
  switch (yyn) {
    case 4: /* predicates: predicates error ';'  */
#line 116 "ortools/flatzinc/parser.yy"
    {
      yyerrok;
    }
#line 1594 "parser.tab.cc"
    break;

    case 19: /* variable_or_constant_declaration: domain ':' IDENTIFIER
                annotations '=' const_literal  */
#line 151 "ortools/flatzinc/parser.yy"
    {
      // Declaration of a (named) constant: we simply register it in the
      // parser's context, and don't store it in the model.
      const Domain& domain = (yyvsp[-5].domain);
      const std::string& identifier = (yyvsp[-3].string_value);
      const Domain& assignment = (yyvsp[0].domain);
      std::vector<Annotation>* const annotations = (yyvsp[-2].annotations);

      if (!assignment.HasOneValue()) {
        // TODO(user): Check that the assignment is included in the domain.
        context->domain_map[identifier] = assignment;
      } else {
        const int64_t value = assignment.values.front();
        CHECK(domain.Contains(value));
        context->integer_map[identifier] = value;
      }
      delete annotations;
    }
#line 1618 "parser.tab.cc"
    break;

    case 20: /* variable_or_constant_declaration: ARRAY '[' IVALUE DOTDOT IVALUE
                ']' OF int_domain ':' IDENTIFIER annotations '=' '[' integers
                ']'  */
#line 171 "ortools/flatzinc/parser.yy"
    {
      std::vector<Annotation>* const annotations = (yyvsp[-4].annotations);
      // Declaration of a (named) constant array. See rule right above.
      CHECK_EQ((yyvsp[-12].integer_value), 1)
          << "Only [1..n] array are supported here.";
      const int64_t num_constants = (yyvsp[-10].integer_value);
      const std::string& identifier = (yyvsp[-5].string_value);
      const std::vector<int64_t>* const assignments = (yyvsp[-1].integers);
      CHECK(assignments != nullptr);
      CHECK_EQ(num_constants, assignments->size());
      // TODO(user): CHECK all values within domain.
      context->integer_array_map[identifier] = *assignments;
      delete assignments;
      delete annotations;
    }
#line 1637 "parser.tab.cc"
    break;

    case 21: /* variable_or_constant_declaration: ARRAY '[' IVALUE DOTDOT IVALUE
                ']' OF int_domain ':' IDENTIFIER annotations '=' '[' ']'  */
#line 186 "ortools/flatzinc/parser.yy"
    {
      std::vector<Annotation>* const annotations = (yyvsp[-3].annotations);
      // Declaration of a (named) constant array. See rule right above.
      CHECK_EQ((yyvsp[-11].integer_value), 1)
          << "Only [1..n] array are supported here.";
      const int64_t num_constants = (yyvsp[-9].integer_value);
      CHECK_EQ(num_constants, 0) << "Empty arrays should have a size of 0";
      const std::string& identifier = (yyvsp[-4].string_value);
      context->integer_array_map[identifier] = std::vector<int64_t>();
      delete annotations;
    }
#line 1652 "parser.tab.cc"
    break;

    case 22: /* variable_or_constant_declaration: ARRAY '[' IVALUE DOTDOT IVALUE
                ']' OF float_domain ':' IDENTIFIER annotations '=' '[' floats
                ']'  */
#line 197 "ortools/flatzinc/parser.yy"
    {
      std::vector<Annotation>* const annotations = (yyvsp[-4].annotations);
      // Declaration of a (named) constant array. See rule right above.
      CHECK_EQ((yyvsp[-12].integer_value), 1)
          << "Only [1..n] array are supported here.";
      const int64_t num_constants = (yyvsp[-10].integer_value);
      const std::string& identifier = (yyvsp[-5].string_value);
      const std::vector<double>* const assignments = (yyvsp[-1].doubles);
      CHECK(assignments != nullptr);
      CHECK_EQ(num_constants, assignments->size());
      // TODO(user): CHECK all values within domain.
      context->float_array_map[identifier] = *assignments;
      delete assignments;
      delete annotations;
    }
#line 1671 "parser.tab.cc"
    break;

    case 23: /* variable_or_constant_declaration: ARRAY '[' IVALUE DOTDOT IVALUE
                ']' OF float_domain ':' IDENTIFIER annotations '=' '[' ']'  */
#line 212 "ortools/flatzinc/parser.yy"
    {
      std::vector<Annotation>* const annotations = (yyvsp[-3].annotations);
      // Declaration of a (named) constant array. See rule right above.
      CHECK_EQ((yyvsp[-11].integer_value), 1)
          << "Only [1..n] array are supported here.";
      const int64_t num_constants = (yyvsp[-9].integer_value);
      CHECK_EQ(num_constants, 0) << "Empty arrays should have a size of 0";
      const std::string& identifier = (yyvsp[-4].string_value);
      context->float_array_map[identifier] = std::vector<double>();
      delete annotations;
    }
#line 1686 "parser.tab.cc"
    break;

    case 24: /* variable_or_constant_declaration: ARRAY '[' IVALUE DOTDOT IVALUE
                ']' OF set_domain ':' IDENTIFIER annotations '=' '['
                const_literals ']'  */
#line 223 "ortools/flatzinc/parser.yy"
    {
      // Declaration of a (named) constant array: See rule above.
      CHECK_EQ((yyvsp[-12].integer_value), 1)
          << "Only [1..n] array are supported here.";
      const int64_t num_constants = (yyvsp[-10].integer_value);
      const Domain& domain = (yyvsp[-7].domain);
      const std::string& identifier = (yyvsp[-5].string_value);
      const std::vector<Domain>* const assignments = (yyvsp[-1].domains);
      const std::vector<Annotation>* const annotations =
          (yyvsp[-4].annotations);
      CHECK(assignments != nullptr);
      CHECK_EQ(num_constants, assignments->size());

      if (!AllDomainsHaveOneValue(*assignments)) {
        context->domain_array_map[identifier] = *assignments;
        // TODO(user): check that all assignments are included in the domain.
      } else {
        std::vector<int64_t> values(num_constants);
        for (int i = 0; i < num_constants; ++i) {
          values[i] = (*assignments)[i].values.front();
          CHECK(domain.Contains(values[i]));
        }
        context->integer_array_map[identifier] = values;
      }
      delete assignments;
      delete annotations;
    }
#line 1716 "parser.tab.cc"
    break;

    case 25: /* variable_or_constant_declaration: VAR domain ':' IDENTIFIER
                annotations optional_var_or_value  */
#line 248 "ortools/flatzinc/parser.yy"
    {
      // Declaration of a variable. If it's unassigned or assigned to a
      // constant, we'll create a new var stored in the model. If it's
      // assigned to another variable x then we simply adjust that
      // existing variable x according to the current (re-)declaration.
      const Domain& domain = (yyvsp[-4].domain);
      const std::string& identifier = (yyvsp[-2].string_value);
      std::vector<Annotation>* const annotations = (yyvsp[-1].annotations);
      const VarRefOrValue& assignment = (yyvsp[0].var_or_value);
      const bool introduced = ContainsId(annotations, "var_is_introduced") ||
                              absl::StartsWith(identifier, "X_INTRODUCED");
      Variable* var = nullptr;
      if (!assignment.defined) {
        var = model->AddVariable(identifier, domain, introduced);
        CHECK_EQ(var->domain.is_a_set, domain.is_a_set);
      } else if (assignment.variable == nullptr) {  // A constant.
        if (assignment.is_float) {
          // Assigned to an float constant.
          const double value = assignment.float_value;
          var = model->AddVariable(identifier, Domain::FloatValue(value),
                                   introduced);
        } else if (assignment.is_domain) {
          // TODO(user): Check that the assignment is included in the domain.
          // We force the set domain because we can have the following code:
          //   var set of {0,18}: x = {0,18};
          // where the second domain is not tagged as a set.
          //
          // Assigned to a set constant.
          var = model->AddVariable(identifier, assignment.domain, introduced,
                                   domain.is_a_set);
        } else {
          CHECK(domain.Contains(assignment.value));
          var = model->AddVariable(
              identifier, Domain::IntegerValue(assignment.value), introduced);
        }
      } else {  // a variable.
        var = assignment.variable;
        var->Merge(identifier, domain, introduced);
      }

      // We also register the variable in the parser's context, and add some
      // output to the model if needed.
      context->variable_map[identifier] = var;
      if (ContainsId(annotations, "output_var")) {
        model->AddOutput(SolutionOutputSpecs::SingleVariable(
            identifier, var, domain.display_as_boolean));
      }
      delete annotations;
    }
#line 1771 "parser.tab.cc"
    break;

    case 26: /* variable_or_constant_declaration: ARRAY '[' IVALUE DOTDOT IVALUE
                ']' OF VAR domain ':' IDENTIFIER annotations
                optional_var_or_value_array  */
#line 299 "ortools/flatzinc/parser.yy"
    {
      // Declaration of a "variable array": these is exactly like N simple
      // variable declarations, where the identifier for declaration #i is
      // IDENTIFIER[i] (1-based index).
      CHECK_EQ((yyvsp[-10].integer_value), 1);
      const int64_t num_vars = (yyvsp[-8].integer_value);
      const Domain& domain = (yyvsp[-4].domain);
      const std::string& identifier = (yyvsp[-2].string_value);
      std::vector<Annotation>* const annotations = (yyvsp[-1].annotations);
      std::vector<VarRefOrValue>* const assignments =
          (yyvsp[0].var_or_value_array);
      CHECK(assignments == nullptr || assignments->size() == num_vars);
      const bool introduced = ContainsId(annotations, "var_is_introduced") ||
                              absl::StartsWith(identifier, "X_INTRODUCED");

      std::vector<Variable*> vars(num_vars, nullptr);

      for (int i = 0; i < num_vars; ++i) {
        const std::string var_name =
            absl::StrFormat("%s[%d]", identifier, i + 1);
        if (assignments == nullptr) {
          vars[i] = model->AddVariable(var_name, domain, introduced);
        } else if ((*assignments)[i].variable == nullptr) {  // A constant.
          if ((*assignments)[i].is_float) {
            // Assigned to an float constant.
            const double value = (*assignments)[i].float_value;
            vars[i] = model->AddVariable(var_name, Domain::FloatValue(value),
                                         introduced);
          } else if ((*assignments)[i].is_domain) {
            // TODO(user): Check that the assignment is included in the domain.
            // We force the set domain because we can have the following code:
            //   var set of {0,18}: x = {0,18};
            // where the second domain is not tagged as a set.
            //
            // Assigned to a set constant.
            vars[i] = model->AddVariable(var_name, (*assignments)[i].domain,
                                         introduced, domain.is_a_set);
          } else {
            // Assigned to an integer constant.
            const int64_t value = (*assignments)[i].value;
            CHECK(domain.Contains(value));
            vars[i] = model->AddVariable(var_name, Domain::IntegerValue(value),
                                         introduced);
          }
        } else {
          Variable* const var = (*assignments)[i].variable;
          CHECK(var != nullptr);
          vars[i] = var;
          vars[i]->Merge(var_name, domain, introduced);
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
              bounds.emplace_back(SolutionOutputSpecs::Bounds(
                  bound.interval_min, bound.interval_max));
            }
            // We add the output information.
            model->AddOutput(SolutionOutputSpecs::MultiDimensionalArray(
                identifier, bounds, vars, domain.display_as_boolean));
          }
        }
        delete annotations;
      }
    }
#line 1858 "parser.tab.cc"
    break;

    case 27: /* optional_var_or_value: '=' var_or_value  */
#line 383 "ortools/flatzinc/parser.yy"
    {
      (yyval.var_or_value) = (yyvsp[0].var_or_value);
    }
#line 1864 "parser.tab.cc"
    break;

    case 28: /* optional_var_or_value: %empty  */
#line 384 "ortools/flatzinc/parser.yy"
    {
      (yyval.var_or_value) = VarRefOrValue::Undefined();
    }
#line 1870 "parser.tab.cc"
    break;

    case 29: /* optional_var_or_value_array: '=' '[' var_or_value_array ']'  */
#line 387 "ortools/flatzinc/parser.yy"
    {
      (yyval.var_or_value_array) = (yyvsp[-1].var_or_value_array);
    }
#line 1876 "parser.tab.cc"
    break;

    case 30: /* optional_var_or_value_array: '=' '[' ']'  */
#line 388 "ortools/flatzinc/parser.yy"
    {
      (yyval.var_or_value_array) = nullptr;
    }
#line 1882 "parser.tab.cc"
    break;

    case 31: /* optional_var_or_value_array: %empty  */
#line 389 "ortools/flatzinc/parser.yy"
    {
      (yyval.var_or_value_array) = nullptr;
    }
#line 1888 "parser.tab.cc"
    break;

    case 32: /* var_or_value_array: var_or_value_array ',' var_or_value  */
#line 392 "ortools/flatzinc/parser.yy"
    {
      (yyval.var_or_value_array) = (yyvsp[-2].var_or_value_array);
      (yyval.var_or_value_array)->push_back((yyvsp[0].var_or_value));
    }
#line 1897 "parser.tab.cc"
    break;

    case 33: /* var_or_value_array: var_or_value  */
#line 396 "ortools/flatzinc/parser.yy"
    {
      (yyval.var_or_value_array) = new std::vector<VarRefOrValue>();
      (yyval.var_or_value_array)->push_back((yyvsp[0].var_or_value));
    }
#line 1906 "parser.tab.cc"
    break;

    case 34: /* var_or_value: IVALUE  */
#line 402 "ortools/flatzinc/parser.yy"
    {
      (yyval.var_or_value) = VarRefOrValue::Value((yyvsp[0].integer_value));
    }
#line 1912 "parser.tab.cc"
    break;

    case 35: /* var_or_value: DVALUE  */
#line 403 "ortools/flatzinc/parser.yy"
    {
      (yyval.var_or_value) = VarRefOrValue::FloatValue((yyvsp[0].double_value));
    }
#line 1918 "parser.tab.cc"
    break;

    case 36: /* var_or_value: IVALUE DOTDOT IVALUE  */
#line 404 "ortools/flatzinc/parser.yy"
    {
      (yyval.var_or_value) = VarRefOrValue::DomainValue(Domain::Interval(
          (yyvsp[-2].integer_value), (yyvsp[0].integer_value)));
    }
#line 1926 "parser.tab.cc"
    break;

    case 37: /* var_or_value: '{' integers '}'  */
#line 407 "ortools/flatzinc/parser.yy"
    {
      CHECK((yyvsp[-1].integers) != nullptr);
      (yyval.var_or_value) = VarRefOrValue::DomainValue(
          Domain::IntegerList(std::move(*(yyvsp[-1].integers))));
      delete (yyvsp[-1].integers);
    }
#line 1936 "parser.tab.cc"
    break;

    case 38: /* var_or_value: IDENTIFIER  */
#line 412 "ortools/flatzinc/parser.yy"
    {
      // A reference to an existing integer constant or variable.
      const std::string& id = (yyvsp[0].string_value);
      if (context->integer_map.contains(id)) {
        (yyval.var_or_value) =
            VarRefOrValue::Value(context->integer_map.at(id));
      } else if (context->float_map.contains(id)) {
        (yyval.var_or_value) =
            VarRefOrValue::FloatValue(context->float_map.at(id));
      } else if (context->variable_map.contains(id)) {
        (yyval.var_or_value) =
            VarRefOrValue::VarRef(context->variable_map.at(id));
      } else {
        LOG(ERROR) << "Unknown symbol " << id;
        (yyval.var_or_value) = VarRefOrValue::Undefined();
        *ok = false;
      }
    }
#line 1956 "parser.tab.cc"
    break;

    case 39: /* var_or_value: IDENTIFIER '[' IVALUE ']'  */
#line 427 "ortools/flatzinc/parser.yy"
    {
      // A given element of an existing constant array or variable array.
      const std::string& id = (yyvsp[-3].string_value);
      const int64_t value = (yyvsp[-1].integer_value);
      if (context->integer_array_map.contains(id)) {
        (yyval.var_or_value) = VarRefOrValue::Value(
            Lookup(context->integer_array_map.at(id), value));
      } else if (context->float_array_map.contains(id)) {
        (yyval.var_or_value) = VarRefOrValue::FloatValue(
            Lookup(context->float_array_map.at(id), value));
      } else if (context->variable_array_map.contains(id)) {
        (yyval.var_or_value) = VarRefOrValue::VarRef(
            Lookup(context->variable_array_map.at(id), value));
      } else {
        LOG(ERROR) << "Unknown symbol " << id;
        (yyval.var_or_value) = VarRefOrValue::Undefined();
        *ok = false;
      }
    }
#line 1980 "parser.tab.cc"
    break;

    case 40: /* int_domain: TOKEN_BOOL  */
#line 448 "ortools/flatzinc/parser.yy"
    {
      (yyval.domain) = Domain::Boolean();
    }
#line 1986 "parser.tab.cc"
    break;

    case 41: /* int_domain: TOKEN_INT  */
#line 449 "ortools/flatzinc/parser.yy"
    {
      (yyval.domain) = Domain::AllInt64();
    }
#line 1992 "parser.tab.cc"
    break;

    case 42: /* int_domain: IVALUE DOTDOT IVALUE  */
#line 450 "ortools/flatzinc/parser.yy"
    {
      (yyval.domain) =
          Domain::Interval((yyvsp[-2].integer_value), (yyvsp[0].integer_value));
    }
#line 1998 "parser.tab.cc"
    break;

    case 43: /* int_domain: '{' integers '}'  */
#line 451 "ortools/flatzinc/parser.yy"
    {
      CHECK((yyvsp[-1].integers) != nullptr);
      (yyval.domain) = Domain::IntegerList(std::move(*(yyvsp[-1].integers)));
      delete (yyvsp[-1].integers);
    }
#line 2008 "parser.tab.cc"
    break;

    case 44: /* set_domain: SET OF TOKEN_BOOL  */
#line 458 "ortools/flatzinc/parser.yy"
    {
      (yyval.domain) = Domain::SetOfBoolean();
    }
#line 2014 "parser.tab.cc"
    break;

    case 45: /* set_domain: SET OF TOKEN_INT  */
#line 459 "ortools/flatzinc/parser.yy"
    {
      (yyval.domain) = Domain::SetOfAllInt64();
    }
#line 2020 "parser.tab.cc"
    break;

    case 46: /* set_domain: SET OF IVALUE DOTDOT IVALUE  */
#line 460 "ortools/flatzinc/parser.yy"
    {
      (yyval.domain) = Domain::SetOfInterval((yyvsp[-2].integer_value),
                                             (yyvsp[0].integer_value));
    }
#line 2026 "parser.tab.cc"
    break;

    case 47: /* set_domain: SET OF '{' integers '}'  */
#line 461 "ortools/flatzinc/parser.yy"
    {
      CHECK((yyvsp[-1].integers) != nullptr);
      (yyval.domain) =
          Domain::SetOfIntegerList(std::move(*(yyvsp[-1].integers)));
      delete (yyvsp[-1].integers);
    }
#line 2036 "parser.tab.cc"
    break;

    case 48: /* float_domain: TOKEN_FLOAT  */
#line 468 "ortools/flatzinc/parser.yy"
    {
      (yyval.domain) = Domain::AllFloats();
    }
#line 2042 "parser.tab.cc"
    break;

    case 49: /* float_domain: DVALUE DOTDOT DVALUE  */
#line 469 "ortools/flatzinc/parser.yy"
    {
      (yyval.domain) = Domain::FloatInterval((yyvsp[-2].double_value),
                                             (yyvsp[0].double_value));
    }
#line 2050 "parser.tab.cc"
    break;

    case 50: /* domain: int_domain  */
#line 474 "ortools/flatzinc/parser.yy"
    {
      (yyval.domain) = (yyvsp[0].domain);
    }
#line 2056 "parser.tab.cc"
    break;

    case 51: /* domain: set_domain  */
#line 475 "ortools/flatzinc/parser.yy"
    {
      (yyval.domain) = (yyvsp[0].domain);
    }
#line 2062 "parser.tab.cc"
    break;

    case 52: /* domain: float_domain  */
#line 476 "ortools/flatzinc/parser.yy"
    {
      (yyval.domain) = (yyvsp[0].domain);
    }
#line 2068 "parser.tab.cc"
    break;

    case 53: /* integers: integers ',' integer  */
#line 479 "ortools/flatzinc/parser.yy"
    {
      (yyval.integers) = (yyvsp[-2].integers);
      (yyval.integers)->emplace_back((yyvsp[0].integer_value));
    }
#line 2074 "parser.tab.cc"
    break;

    case 54: /* integers: integer  */
#line 480 "ortools/flatzinc/parser.yy"
    {
      (yyval.integers) = new std::vector<int64_t>();
      (yyval.integers)->emplace_back((yyvsp[0].integer_value));
    }
#line 2080 "parser.tab.cc"
    break;

    case 55: /* integer: IVALUE  */
#line 483 "ortools/flatzinc/parser.yy"
    {
      (yyval.integer_value) = (yyvsp[0].integer_value);
    }
#line 2086 "parser.tab.cc"
    break;

    case 56: /* integer: IDENTIFIER  */
#line 484 "ortools/flatzinc/parser.yy"
    {
      (yyval.integer_value) = context->integer_map.at((yyvsp[0].string_value));
    }
#line 2092 "parser.tab.cc"
    break;

    case 57: /* integer: IDENTIFIER '[' IVALUE ']'  */
#line 485 "ortools/flatzinc/parser.yy"
    {
      (yyval.integer_value) =
          Lookup(context->integer_array_map.at((yyvsp[-3].string_value)),
                 (yyvsp[-1].integer_value));
    }
#line 2100 "parser.tab.cc"
    break;

    case 58: /* floats: floats ',' float  */
#line 490 "ortools/flatzinc/parser.yy"
    {
      (yyval.doubles) = (yyvsp[-2].doubles);
      (yyval.doubles)->emplace_back((yyvsp[0].double_value));
    }
#line 2106 "parser.tab.cc"
    break;

    case 59: /* floats: float  */
#line 491 "ortools/flatzinc/parser.yy"
    {
      (yyval.doubles) = new std::vector<double>();
      (yyval.doubles)->emplace_back((yyvsp[0].double_value));
    }
#line 2112 "parser.tab.cc"
    break;

    case 60: /* float: DVALUE  */
#line 494 "ortools/flatzinc/parser.yy"
    {
      (yyval.double_value) = (yyvsp[0].double_value);
    }
#line 2118 "parser.tab.cc"
    break;

    case 61: /* float: IDENTIFIER  */
#line 495 "ortools/flatzinc/parser.yy"
    {
      (yyval.double_value) = context->float_map.at((yyvsp[0].string_value));
    }
#line 2124 "parser.tab.cc"
    break;

    case 62: /* float: IDENTIFIER '[' IVALUE ']'  */
#line 496 "ortools/flatzinc/parser.yy"
    {
      (yyval.double_value) =
          Lookup(context->float_array_map.at((yyvsp[-3].string_value)),
                 (yyvsp[-1].integer_value));
    }
#line 2132 "parser.tab.cc"
    break;

    case 63: /* const_literal: IVALUE  */
#line 501 "ortools/flatzinc/parser.yy"
    {
      (yyval.domain) = Domain::IntegerValue((yyvsp[0].integer_value));
    }
#line 2138 "parser.tab.cc"
    break;

    case 64: /* const_literal: IVALUE DOTDOT IVALUE  */
#line 502 "ortools/flatzinc/parser.yy"
    {
      (yyval.domain) =
          Domain::Interval((yyvsp[-2].integer_value), (yyvsp[0].integer_value));
    }
#line 2144 "parser.tab.cc"
    break;

    case 65: /* const_literal: '{' integers '}'  */
#line 503 "ortools/flatzinc/parser.yy"
    {
      CHECK((yyvsp[-1].integers) != nullptr);
      (yyval.domain) = Domain::IntegerList(std::move(*(yyvsp[-1].integers)));
      delete (yyvsp[-1].integers);
    }
#line 2154 "parser.tab.cc"
    break;

    case 66: /* const_literal: '{' '}'  */
#line 508 "ortools/flatzinc/parser.yy"
    {
      (yyval.domain) = Domain::EmptyDomain();
    }
#line 2160 "parser.tab.cc"
    break;

    case 67: /* const_literal: DVALUE  */
#line 509 "ortools/flatzinc/parser.yy"
    {
      (yyval.domain) = Domain::FloatValue((yyvsp[0].double_value));
    }
#line 2168 "parser.tab.cc"
    break;

    case 68: /* const_literal: IDENTIFIER  */
#line 512 "ortools/flatzinc/parser.yy"
    {
      (yyval.domain) = Domain::IntegerValue(
          context->integer_map.at((yyvsp[0].string_value)));
    }
#line 2174 "parser.tab.cc"
    break;

    case 69: /* const_literal: IDENTIFIER '[' IVALUE ']'  */
#line 513 "ortools/flatzinc/parser.yy"
    {
      (yyval.domain) = Domain::IntegerValue(
          Lookup(context->integer_array_map.at((yyvsp[-3].string_value)),
                 (yyvsp[-1].integer_value)));
    }
#line 2183 "parser.tab.cc"
    break;

    case 70: /* const_literals: const_literals ',' const_literal  */
#line 519 "ortools/flatzinc/parser.yy"
    {
      (yyval.domains) = (yyvsp[-2].domains);
      (yyval.domains)->emplace_back((yyvsp[0].domain));
    }
#line 2192 "parser.tab.cc"
    break;

    case 71: /* const_literals: const_literal  */
#line 523 "ortools/flatzinc/parser.yy"
    {
      (yyval.domains) = new std::vector<Domain>();
      (yyval.domains)->emplace_back((yyvsp[0].domain));
    }
#line 2198 "parser.tab.cc"
    break;

    case 74: /* constraint: CONSTRAINT IDENTIFIER '(' arguments ')' annotations
              */
#line 533 "ortools/flatzinc/parser.yy"
    {
      const std::string& identifier = (yyvsp[-4].string_value);
      CHECK((yyvsp[-2].args) != nullptr) << "Missing argument in constraint";
      const std::vector<Argument>& arguments = *(yyvsp[-2].args);
      std::vector<Annotation>* const annotations = (yyvsp[0].annotations);

      model->AddConstraint(identifier, arguments,
                           ContainsId(annotations, "domain"),
                           ContainsId(annotations, "symmetry_breaking"),
                           ContainsId(annotations, "redundant"));
      delete annotations;
      delete (yyvsp[-2].args);
    }
#line 2213 "parser.tab.cc"
    break;

    case 75: /* arguments: arguments ',' argument  */
#line 545 "ortools/flatzinc/parser.yy"
    {
      (yyval.args) = (yyvsp[-2].args);
      (yyval.args)->emplace_back((yyvsp[0].arg));
    }
#line 2219 "parser.tab.cc"
    break;

    case 76: /* arguments: argument  */
#line 546 "ortools/flatzinc/parser.yy"
    {
      (yyval.args) = new std::vector<Argument>();
      (yyval.args)->emplace_back((yyvsp[0].arg));
    }
#line 2225 "parser.tab.cc"
    break;

    case 77: /* argument: IVALUE  */
#line 549 "ortools/flatzinc/parser.yy"
    {
      (yyval.arg) = Argument::IntegerValue((yyvsp[0].integer_value));
    }
#line 2231 "parser.tab.cc"
    break;

    case 78: /* argument: DVALUE  */
#line 550 "ortools/flatzinc/parser.yy"
    {
      (yyval.arg) = Argument::FloatValue((yyvsp[0].double_value));
    }
#line 2237 "parser.tab.cc"
    break;

    case 79: /* argument: SVALUE  */
#line 551 "ortools/flatzinc/parser.yy"
    {
      (yyval.arg) = Argument::VoidArgument();
    }
#line 2243 "parser.tab.cc"
    break;

    case 80: /* argument: IVALUE DOTDOT IVALUE  */
#line 552 "ortools/flatzinc/parser.yy"
    {
      (yyval.arg) = Argument::Interval((yyvsp[-2].integer_value),
                                       (yyvsp[0].integer_value));
    }
#line 2249 "parser.tab.cc"
    break;

    case 81: /* argument: '{' integers '}'  */
#line 553 "ortools/flatzinc/parser.yy"
    {
      CHECK((yyvsp[-1].integers) != nullptr);
      (yyval.arg) = Argument::IntegerList(std::move(*(yyvsp[-1].integers)));
      delete (yyvsp[-1].integers);
    }
#line 2259 "parser.tab.cc"
    break;

    case 82: /* argument: IDENTIFIER  */
#line 558 "ortools/flatzinc/parser.yy"
    {
      const std::string& id = (yyvsp[0].string_value);
      if (context->integer_map.contains(id)) {
        (yyval.arg) = Argument::IntegerValue(context->integer_map.at(id));
      } else if (context->integer_array_map.contains(id)) {
        (yyval.arg) = Argument::IntegerList(context->integer_array_map.at(id));
      } else if (context->float_map.contains(id)) {
        const double d = context->float_map.at(id);
        (yyval.arg) = Argument::FloatValue(d);
      } else if (context->float_array_map.contains(id)) {
        const auto& double_values = context->float_array_map.at(id);
        (yyval.arg) = Argument::FloatList(std::move(double_values));
      } else if (context->variable_map.contains(id)) {
        (yyval.arg) = Argument::VarRef(context->variable_map.at(id));
      } else if (context->variable_array_map.contains(id)) {
        (yyval.arg) = Argument::VarRefArray(context->variable_array_map.at(id));
      } else if (context->domain_map.contains(id)) {
        const Domain& d = context->domain_map.at(id);
        (yyval.arg) = Argument::FromDomain(d);
      } else {
        CHECK(context->domain_array_map.contains(id))
            << "Unknown identifier: " << id;
        const std::vector<Domain>& d = context->domain_array_map.at(id);
        (yyval.arg) = Argument::DomainList(d);
      }
    }
#line 2290 "parser.tab.cc"
    break;

    case 83: /* argument: IDENTIFIER '[' IVALUE ']'  */
#line 584 "ortools/flatzinc/parser.yy"
    {
      const std::string& id = (yyvsp[-3].string_value);
      const int64_t index = (yyvsp[-1].integer_value);
      if (context->integer_array_map.contains(id)) {
        (yyval.arg) = Argument::IntegerValue(
            Lookup(context->integer_array_map.at(id), index));
      } else if (context->variable_array_map.contains(id)) {
        (yyval.arg) =
            Argument::VarRef(Lookup(context->variable_array_map.at(id), index));
      } else {
        CHECK(context->domain_array_map.contains(id))
            << "Unknown identifier: " << id;
        const Domain& d = Lookup(context->domain_array_map.at(id), index);
        (yyval.arg) = Argument::FromDomain(d);
      }
    }
#line 2312 "parser.tab.cc"
    break;

    case 84: /* argument: '[' var_or_value_array ']'  */
#line 601 "ortools/flatzinc/parser.yy"
    {
      std::vector<VarRefOrValue>* const arguments =
          (yyvsp[-1].var_or_value_array);
      CHECK(arguments != nullptr);
      bool has_variables = false;
      bool has_floats = false;
      for (int i = 0; i < arguments->size(); ++i) {
        if ((*arguments)[i].variable != nullptr) {
          has_variables = true;
        }
        if ((*arguments)[i].is_float) {
          has_floats = true;
        }
      }
      if (has_variables) {
        std::vector<Variable*> vars;
        vars.reserve(arguments->size());
        for (int i = 0; i < arguments->size(); ++i) {
          const VarRefOrValue data = (*arguments)[i];
          if (data.variable != nullptr) {
            vars.push_back(data.variable);
          } else if (!data.is_float) {
            vars.push_back(model->AddConstant(data.value));
          } else {
            vars.push_back(model->AddFloatConstant(data.float_value));
          }
        }
        (yyval.arg) = Argument::VarRefArray(std::move(vars));
      } else if (has_floats) {
        std::vector<double> values;
        values.reserve(arguments->size());
        for (const VarRefOrValue& data : *arguments) {
          if (data.is_float) {
            values.push_back(data.float_value);
          } else {
            values.push_back(data.value);
          }
        }
        (yyval.arg) = Argument::FloatList(std::move(values));
      } else {
        std::vector<int64_t> values;
        values.reserve(arguments->size());
        for (const VarRefOrValue& data : *arguments) {
          values.push_back(data.value);
        }
        (yyval.arg) = Argument::IntegerList(std::move(values));
      }
      delete arguments;
    }
#line 2365 "parser.tab.cc"
    break;

    case 85: /* argument: '[' ']'  */
#line 649 "ortools/flatzinc/parser.yy"
    {
      (yyval.arg) = Argument::VoidArgument();
    }
#line 2373 "parser.tab.cc"
    break;

    case 86: /* annotations: annotations COLONCOLON annotation  */
#line 658 "ortools/flatzinc/parser.yy"
    {
      (yyval.annotations) = (yyvsp[-2].annotations) != nullptr
                                ? (yyvsp[-2].annotations)
                                : new std::vector<Annotation>();
      (yyval.annotations)->emplace_back((yyvsp[0].annotation));
    }
#line 2382 "parser.tab.cc"
    break;

    case 87: /* annotations: %empty  */
#line 662 "ortools/flatzinc/parser.yy"
    {
      (yyval.annotations) = nullptr;
    }
#line 2388 "parser.tab.cc"
    break;

    case 88: /* annotation_arguments: annotation_arguments ',' annotation  */
#line 665 "ortools/flatzinc/parser.yy"
    {
      (yyval.annotations) = (yyvsp[-2].annotations);
      (yyval.annotations)->emplace_back((yyvsp[0].annotation));
    }
#line 2394 "parser.tab.cc"
    break;

    case 89: /* annotation_arguments: annotation  */
#line 666 "ortools/flatzinc/parser.yy"
    {
      (yyval.annotations) = new std::vector<Annotation>();
      (yyval.annotations)->emplace_back((yyvsp[0].annotation));
    }
#line 2400 "parser.tab.cc"
    break;

    case 90: /* annotation: IVALUE DOTDOT IVALUE  */
#line 669 "ortools/flatzinc/parser.yy"
    {
      (yyval.annotation) = Annotation::Interval((yyvsp[-2].integer_value),
                                                (yyvsp[0].integer_value));
    }
#line 2406 "parser.tab.cc"
    break;

    case 91: /* annotation: IVALUE  */
#line 670 "ortools/flatzinc/parser.yy"
    {
      (yyval.annotation) = Annotation::IntegerValue((yyvsp[0].integer_value));
    }
#line 2412 "parser.tab.cc"
    break;

    case 92: /* annotation: SVALUE  */
#line 671 "ortools/flatzinc/parser.yy"
    {
      (yyval.annotation) = Annotation::String((yyvsp[0].string_value));
    }
#line 2418 "parser.tab.cc"
    break;

    case 93: /* annotation: IDENTIFIER  */
#line 672 "ortools/flatzinc/parser.yy"
    {
      const std::string& id = (yyvsp[0].string_value);
      if (context->variable_map.contains(id)) {
        (yyval.annotation) = Annotation::VarRef(context->variable_map.at(id));
      } else if (context->variable_array_map.contains(id)) {
        (yyval.annotation) =
            Annotation::VarRefArray(context->variable_array_map.at(id));
      } else if (context->integer_map.contains(id)) {
        (yyval.annotation) =
            Annotation::IntegerValue(context->integer_map.at(id));
      } else if (context->integer_array_map.contains(id)) {
        (yyval.annotation) =
            Annotation::IntegerList(context->integer_array_map.at(id));
      } else {
        (yyval.annotation) = Annotation::Identifier(id);
      }
    }
#line 2437 "parser.tab.cc"
    break;

    case 94: /* annotation: IDENTIFIER '(' annotation_arguments ')'  */
#line 686 "ortools/flatzinc/parser.yy"
    {
      std::vector<Annotation>* const annotations = (yyvsp[-1].annotations);
      if (annotations != nullptr) {
        (yyval.annotation) = Annotation::FunctionCallWithArguments(
            (yyvsp[-3].string_value), std::move(*annotations));
        delete annotations;
      } else {
        (yyval.annotation) = Annotation::FunctionCall((yyvsp[-3].string_value));
      }
    }
#line 2451 "parser.tab.cc"
    break;

    case 95: /* annotation: IDENTIFIER '[' IVALUE ']'  */
#line 695 "ortools/flatzinc/parser.yy"
    {
      CHECK(context->variable_array_map.contains((yyvsp[-3].string_value)))
          << "Unknown identifier: " << (yyvsp[-3].string_value);
      (yyval.annotation) = Annotation::VarRef(
          Lookup(context->variable_array_map.at((yyvsp[-3].string_value)),
                 (yyvsp[-1].integer_value)));
    }
#line 2462 "parser.tab.cc"
    break;

    case 96: /* annotation: '[' annotation_arguments ']'  */
#line 701 "ortools/flatzinc/parser.yy"
    {
      std::vector<Annotation>* const annotations = (yyvsp[-1].annotations);
      if (annotations != nullptr && !annotations->empty()) {
        bool all_integers = true;
        bool all_vars = true;
        for (const Annotation& ann : *annotations) {
          if (ann.type != Annotation::INT_VALUE) all_integers = false;
          if (ann.type != Annotation::VAR_REF) all_vars = false;
        }
        if (all_integers) {
          std::vector<int64_t> values;
          for (const Annotation& ann : *annotations) {
            values.push_back(ann.interval_min);
          }
          (yyval.annotation) = Annotation::IntegerList(values);
        } else if (all_vars) {
          std::vector<Variable*> vars;
          for (const Annotation& ann : *annotations) {
            vars.push_back(ann.variables[0]);
          }
          (yyval.annotation) = Annotation::VarRefArray(vars);
        } else {
          (yyval.annotation) =
              Annotation::AnnotationList(std::move(*annotations));
        }
        delete annotations;
      } else {
        (yyval.annotation) = Annotation::Empty();
      }
    }
#line 2496 "parser.tab.cc"
    break;

    case 97: /* annotation: '[' ']'  */
#line 730 "ortools/flatzinc/parser.yy"
    {
      (yyval.annotation) = Annotation::Empty();
    }
#line 2504 "parser.tab.cc"
    break;

    case 98: /* solve: SOLVE annotations SATISFY  */
#line 739 "ortools/flatzinc/parser.yy"
    {
      if ((yyvsp[-1].annotations) != nullptr) {
        model->Satisfy(std::move(*(yyvsp[-1].annotations)));
        delete (yyvsp[-1].annotations);
      } else {
        model->Satisfy(std::vector<Annotation>());
      }
    }
#line 2517 "parser.tab.cc"
    break;

    case 99: /* solve: SOLVE annotations MINIMIZE argument  */
#line 747 "ortools/flatzinc/parser.yy"
    {
      Variable* obj_var = (yyvsp[0].arg).type == Argument::VAR_REF
                              ? (yyvsp[0].arg).Var()
                              : model->AddConstant((yyvsp[0].arg).Value());
      if ((yyvsp[-2].annotations) != nullptr) {
        model->Minimize(obj_var, std::move(*(yyvsp[-2].annotations)));
        delete (yyvsp[-2].annotations);
      } else {
        model->Minimize(obj_var, std::vector<Annotation>());
      }
    }
#line 2533 "parser.tab.cc"
    break;

    case 100: /* solve: SOLVE annotations MAXIMIZE argument  */
#line 758 "ortools/flatzinc/parser.yy"
    {
      Variable* obj_var = (yyvsp[0].arg).type == Argument::VAR_REF
                              ? (yyvsp[0].arg).Var()
                              : model->AddConstant((yyvsp[0].arg).Value());
      if ((yyvsp[-2].annotations) != nullptr) {
        model->Maximize(obj_var, std::move(*(yyvsp[-2].annotations)));
        delete (yyvsp[-2].annotations);
      } else {
        model->Maximize(obj_var, std::vector<Annotation>());
      }
    }
#line 2549 "parser.tab.cc"
    break;

#line 2553 "parser.tab.cc"

    default:
      break;
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
  YY_SYMBOL_PRINT("-> $$ =", YY_CAST(yysymbol_kind_t, yyr1[yyn]), &yyval,
                  &yyloc);

  YYPOPSTACK(yylen);
  yylen = 0;

  *++yyvsp = yyval;

  /* Now 'shift' the result of the reduction.  Determine what state
     that goes to, based on the state we popped back to and the rule
     number reduced by.  */
  {
    const int yylhs = yyr1[yyn] - YYNTOKENS;
    const int yyi = yypgoto[yylhs] + *yyssp;
    yystate = (0 <= yyi && yyi <= YYLAST && yycheck[yyi] == *yyssp
                   ? yytable[yyi]
                   : yydefgoto[yylhs]);
  }

  goto yynewstate;

/*--------------------------------------.
| yyerrlab -- here on detecting error.  |
`--------------------------------------*/
yyerrlab:
  /* Make sure we have latest lookahead translation.  See comments at
     user semantic actions for why this is necessary.  */
  yytoken = yychar == ORFZ_EMPTY ? YYSYMBOL_YYEMPTY : YYTRANSLATE(yychar);
  /* If not already recovering from an error, report this error.  */
  if (!yyerrstatus) {
    ++yynerrs;
    {
      yypcontext_t yyctx = {yyssp, yytoken};
      char const* yymsgp = YY_("syntax error");
      int yysyntax_error_status;
      yysyntax_error_status = yysyntax_error(&yymsg_alloc, &yymsg, &yyctx);
      if (yysyntax_error_status == 0)
        yymsgp = yymsg;
      else if (yysyntax_error_status == -1) {
        if (yymsg != yymsgbuf) YYSTACK_FREE(yymsg);
        yymsg = YY_CAST(char*, YYSTACK_ALLOC(YY_CAST(YYSIZE_T, yymsg_alloc)));
        if (yymsg) {
          yysyntax_error_status = yysyntax_error(&yymsg_alloc, &yymsg, &yyctx);
          yymsgp = yymsg;
        } else {
          yymsg = yymsgbuf;
          yymsg_alloc = sizeof yymsgbuf;
          yysyntax_error_status = YYENOMEM;
        }
      }
      yyerror(context, model, ok, scanner, yymsgp);
      if (yysyntax_error_status == YYENOMEM) YYNOMEM;
    }
  }

  if (yyerrstatus == 3) {
    /* If just tried and failed to reuse lookahead token after an
       error, discard it.  */

    if (yychar <= ORFZ_EOF) {
      /* Return failure if at end of input.  */
      if (yychar == ORFZ_EOF) YYABORT;
    } else {
      yydestruct("Error: discarding", yytoken, &yylval, context, model, ok,
                 scanner);
      yychar = ORFZ_EMPTY;
    }
  }

  /* Else will try to reuse lookahead token after shifting the error
     token.  */
  goto yyerrlab1;

/*---------------------------------------------------.
| yyerrorlab -- error raised explicitly by YYERROR.  |
`---------------------------------------------------*/
yyerrorlab:
  /* Pacify compilers when the user code never invokes YYERROR and the
     label yyerrorlab therefore never appears in user code.  */
  if (0) YYERROR;
  ++yynerrs;

  /* Do not reclaim the symbols of the rule whose action triggered
     this YYERROR.  */
  YYPOPSTACK(yylen);
  yylen = 0;
  YY_STACK_PRINT(yyss, yyssp);
  yystate = *yyssp;
  goto yyerrlab1;

/*-------------------------------------------------------------.
| yyerrlab1 -- common code for both syntax error and YYERROR.  |
`-------------------------------------------------------------*/
yyerrlab1:
  yyerrstatus = 3; /* Each real token shifted decrements this.  */

  /* Pop stack until we find a state that shifts the error token.  */
  for (;;) {
    yyn = yypact[yystate];
    if (!yypact_value_is_default(yyn)) {
      yyn += YYSYMBOL_YYerror;
      if (0 <= yyn && yyn <= YYLAST && yycheck[yyn] == YYSYMBOL_YYerror) {
        yyn = yytable[yyn];
        if (0 < yyn) break;
      }
    }

    /* Pop the current state because it cannot handle the error token.  */
    if (yyssp == yyss) YYABORT;

    yydestruct("Error: popping", YY_ACCESSING_SYMBOL(yystate), yyvsp, context,
               model, ok, scanner);
    YYPOPSTACK(1);
    yystate = *yyssp;
    YY_STACK_PRINT(yyss, yyssp);
  }

  YY_IGNORE_MAYBE_UNINITIALIZED_BEGIN
  *++yyvsp = yylval;
  YY_IGNORE_MAYBE_UNINITIALIZED_END

  /* Shift the error token.  */
  YY_SYMBOL_PRINT("Shifting", YY_ACCESSING_SYMBOL(yyn), yyvsp, yylsp);

  yystate = yyn;
  goto yynewstate;

/*-------------------------------------.
| yyacceptlab -- YYACCEPT comes here.  |
`-------------------------------------*/
yyacceptlab:
  yyresult = 0;
  goto yyreturnlab;

/*-----------------------------------.
| yyabortlab -- YYABORT comes here.  |
`-----------------------------------*/
yyabortlab:
  yyresult = 1;
  goto yyreturnlab;

/*-----------------------------------------------------------.
| yyexhaustedlab -- YYNOMEM (memory exhaustion) comes here.  |
`-----------------------------------------------------------*/
yyexhaustedlab:
  yyerror(context, model, ok, scanner, YY_("memory exhausted"));
  yyresult = 2;
  goto yyreturnlab;

/*----------------------------------------------------------.
| yyreturnlab -- parsing is finished, clean up and return.  |
`----------------------------------------------------------*/
yyreturnlab:
  if (yychar != ORFZ_EMPTY) {
    /* Make sure we have latest lookahead translation.  See comments at
       user semantic actions for why this is necessary.  */
    yytoken = YYTRANSLATE(yychar);
    yydestruct("Cleanup: discarding lookahead", yytoken, &yylval, context,
               model, ok, scanner);
  }
  /* Do not reclaim the symbols of the rule whose action triggered
     this YYABORT or YYACCEPT.  */
  YYPOPSTACK(yylen);
  YY_STACK_PRINT(yyss, yyssp);
  while (yyssp != yyss) {
    yydestruct("Cleanup: popping", YY_ACCESSING_SYMBOL(+*yyssp), yyvsp, context,
               model, ok, scanner);
    YYPOPSTACK(1);
  }
#ifndef yyoverflow
  if (yyss != yyssa) YYSTACK_FREE(yyss);
#endif
  if (yymsg != yymsgbuf) YYSTACK_FREE(yymsg);
  return yyresult;
}

#line 770 "ortools/flatzinc/parser.yy"
