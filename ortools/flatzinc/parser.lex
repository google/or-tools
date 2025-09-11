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

/* Create a reentrant parser. */
%option reentrant
/* Allow parameter passing to and from the bison parser. */
%option bison-bridge
%option noyywrap
%option yylineno
/* Rename yy to orfz_ in public functions. */
%option prefix="orfz_"

%{
#include <cstdint>
#include <string>
#include "absl/strings/numbers.h"
#include "ortools/flatzinc/parser.tab.hh"
#if defined(_MSC_VER)
#define YY_NO_UNISTD_H
#include "io.h"
#define isatty _isatty
#endif
%}

/* Rules that parse the bottom-line string tokens of a .fz file and
   convert them into YACC tokens, which may carry a value. See the
   LexerInfo struct and the %token declarations in ./parser.yy. */

%%
"array"         { return ARRAY;      }
"bool"          { return TOKEN_BOOL; }
"constraint"    { return CONSTRAINT; }
"float"         { return TOKEN_FLOAT;}
"int"           { return TOKEN_INT;  }
"maximize"      { return MAXIMIZE;   }
"minimize"      { return MINIMIZE;   }
"of"            { return OF;         }
"predicate"     { return PREDICATE;  }
"satisfy"       { return SATISFY;    }
"set"           { return SET;        }
"solve"         { return SOLVE;      }
"var"           { return VAR;        }
\.\.            { return DOTDOT;     }
::              { return COLONCOLON; }

"true" {
  yylval->integer_value = 1;
  return IVALUE;
}
"false" {
  yylval->integer_value = 0;
  return IVALUE;
}
-?[0-9]+ {
  CHECK(absl::SimpleAtoi(yytext, &yylval->integer_value));
  return IVALUE;
}
-?0x[0-9A-Fa-f]+ {
  CHECK(absl::SimpleAtoi(yytext, &yylval->integer_value));
  return IVALUE;
}
-?0o[0-7]+ {
  CHECK(absl::SimpleAtoi(yytext, &yylval->integer_value));
  return IVALUE;
}
-?[0-9]+\.[0-9]+ {
  CHECK(absl::SimpleAtod(yytext, &yylval->double_value));
  return DVALUE;
}
-?[0-9]+\.[0-9]+[Ee][+-]?[0-9]+  {
  CHECK(absl::SimpleAtod(yytext, &yylval->double_value));
  return DVALUE;
}
-?[0-9]+[Ee][+-]?[0-9]+ {
  CHECK(absl::SimpleAtod(yytext, &yylval->double_value));
  return DVALUE;

}
[A-Za-z][A-Za-z0-9_]* {
  yylval->string_value = yytext;
  return IDENTIFIER;
}
_[_]*[A-Za-z][A-Za-z0-9_]* {
  yylval->string_value = yytext;
  return IDENTIFIER;
}
\"[^"\n]*\" { yylval->string_value = yytext; return SVALUE; }
\n          ;
[ \t]       ;
%.*         ;
.           { return yytext[0]; }
%%
