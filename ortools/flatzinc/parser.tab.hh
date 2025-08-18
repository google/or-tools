/* A Bison parser, made by GNU Bison 3.8.2.  */

/* DO NOT RELY ON FEATURES THAT ARE NOT DOCUMENTED in the manual,
   especially those whose name start with YY_ or yy_.  They are
   private implementation details that can be changed or removed.  */

#ifndef YY_ORFZ_PARSER_TAB_HH_INCLUDED
# define YY_ORFZ_PARSER_TAB_HH_INCLUDED
/* Debug traces.  */
#ifndef ORFZ_DEBUG
# if defined YYDEBUG
#if YYDEBUG
#   define ORFZ_DEBUG 1
#  else
#   define ORFZ_DEBUG 0
#  endif
# else /* ! defined YYDEBUG */
#  define ORFZ_DEBUG 0
# endif /* ! defined YYDEBUG */
#endif  /* ! defined ORFZ_DEBUG */
#if ORFZ_DEBUG
extern int orfz_debug;
#endif
/* "%code requires" blocks.  */
#line 19 "ortools/flatzinc/parser.yy"

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

#line 73 "parser.tab.hh"

/* Token kinds.  */
#ifndef ORFZ_TOKENTYPE
# define ORFZ_TOKENTYPE
  enum orfz_tokentype
  {
    ORFZ_EMPTY = -2,
    ORFZ_EOF = 0,                  /* "end of file"  */
    ORFZ_error = 256,              /* error  */
    ORFZ_UNDEF = 257,              /* "invalid token"  */
    ARRAY = 258,                   /* ARRAY  */
    TOKEN_BOOL = 259,              /* TOKEN_BOOL  */
    CONSTRAINT = 260,              /* CONSTRAINT  */
    TOKEN_FLOAT = 261,             /* TOKEN_FLOAT  */
    TOKEN_INT = 262,               /* TOKEN_INT  */
    MAXIMIZE = 263,                /* MAXIMIZE  */
    MINIMIZE = 264,                /* MINIMIZE  */
    OF = 265,                      /* OF  */
    PREDICATE = 266,               /* PREDICATE  */
    SATISFY = 267,                 /* SATISFY  */
    SET = 268,                     /* SET  */
    SOLVE = 269,                   /* SOLVE  */
    VAR = 270,                     /* VAR  */
    DOTDOT = 271,                  /* DOTDOT  */
    COLONCOLON = 272,              /* COLONCOLON  */
    IVALUE = 273,                  /* IVALUE  */
    SVALUE = 274,                  /* SVALUE  */
    IDENTIFIER = 275,              /* IDENTIFIER  */
    DVALUE = 276                   /* DVALUE  */
  };
  typedef enum orfz_tokentype orfz_token_kind_t;
#endif

/* Value type.  */




int orfz_parse (operations_research::fz::ParserContext* context, operations_research::fz::Model* model, bool* ok, void* scanner);


#endif /* !YY_ORFZ_PARSER_TAB_HH_INCLUDED  */
