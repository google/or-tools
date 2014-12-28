/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*- */
/*
 *  Main authors:
 *     Guido Tack <tack@gecode.org>
 *  Modified:
 *     Laurent Perron for OR-Tools (laurent.perron@gmail.com)
 *
 *  Copyright:
 *     Guido Tack, 2007
 *
 *  Last modified:
 *     $Date: 2006-12-11 03:27:31 +1100 (Mon, 11 Dec 2006) $ by $Author: schulte $
 *     $Revision: 4024 $
 *
 *  This file is part of Gecode, the generic constraint
 *  development environment:
 *     http://www.gecode.org
 *
 *  Permission is hereby granted, free of charge, to any person obtaining
 *  a copy of this software and associated documentation files (the
 *  "Software"), to deal in the Software without restriction, including
 *  without limitation the rights to use, copy, modify, merge, publish,
 *  distribute, sublicense, and/or sell copies of the Software, and to
 *  permit persons to whom the Software is furnished to do so, subject to
 *  the following conditions:
 *
 *  The above copyright notice and this permission notice shall be
 *  included in all copies or substantial portions of the Software.
 *
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 *  EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 *  MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 *  NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
 *  LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
 *  OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 *  WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 */

%option reentrant
%option bison-bridge
%option noyywrap
%option yylineno
%option prefix="orfz_"

%{

namespace operations_research {
class ParserState;
}  // namespace operations_research

void orfz_error(operations_research::ParserState*, void*, const char*);
#define orfz_error(s) \
  orfz_error(static_cast<operations_research::ParserState*>(yyextra),\
             yyscanner, s)

#include "base/strtoint.h"
#include "old_flatzinc/parser.h"
#include "old_flatzinc/flatzinc.tab.hh"

using operations_research::atoi64;

const char* stringbuf;
int stringbuflen;
int stringbufpos;

int yy_input_proc(char* buf, int size, yyscan_t yyscanner);
#define YY_INPUT(buf, result, max_size) \
  result = yy_input_proc(buf, max_size, yyscanner);
%}

%%

\n                { /*yylineno++;*/ /* ignore EOL */ }
[ \t\r]           { /* ignore whitespace */ }
%.*               { /* ignore comments */ }

"true"            { yylval->iValue = 1; return FZ_BOOL_LIT; }
"false"           { yylval->iValue = 0; return FZ_BOOL_LIT; }
-?[0-9]+          {
    const int64 val = atoi64(yytext);
    yylval->iValue = val;
    return FZ_INT_LIT;
  }
-?0x[0-9A-Fa-f]+  {
    yylval->iValue = atoi64(yytext); return FZ_INT_LIT;
  }
-?0o[0-7]+        {
    yylval->iValue = atoi64(yytext); return FZ_INT_LIT;
  }
-?[0-9]+\.[0-9]+  { yylval->dValue = strtod(yytext,nullptr);
                    return FZ_FLOAT_LIT; }
-?[0-9]+\.[0-9]+[Ee][+-]?[0-9]+  { yylval->dValue = strtod(yytext,nullptr);
                                   return FZ_FLOAT_LIT; }
-?[0-9]+[Ee][+-]?[0-9]+  { yylval->dValue = strtod(yytext,nullptr);
                           return FZ_FLOAT_LIT; }
[=:;{}(),\[\]\.]    { return *yytext; }
\.\.              { return FZ_DOTDOT; }
::                { return FZ_COLONCOLON; }
"annotation"      { return FZ_ANNOTATION; }
"any"             { return FZ_ANY; }
"array"           { return FZ_ARRAY; }
"bool"            { return FZ_BOOL; }
"case"            { return FZ_CASE; }
"constraint"      { return FZ_CONSTRAINT; }
"default"         { return FZ_DEFAULT; }
"else"            { return FZ_ELSE; }
"elseif"          { return FZ_ELSEIF; }
"endif"           { return FZ_ENDIF; }
"enum"            { return FZ_ENUM; }
"float"           { return FZ_FLOAT; }
"function"        { return FZ_FUNCTION; }
"if"              { return FZ_IF; }
"include"         { return FZ_INCLUDE; }
"int"             { return FZ_INT; }
"let"             { return FZ_LET; }
"maximize"        { yylval->bValue = false; return FZ_MAXIMIZE; }
"minimize"        { yylval->bValue = true; return FZ_MINIMIZE; }
"of"              { return FZ_OF; }
"satisfy"         { return FZ_SATISFY; }
"output"          { return FZ_OUTPUT; }
"par"             { yylval->bValue = false; return FZ_PAR; }
"predicate"       { return FZ_PREDICATE; }
"record"          { return FZ_RECORD; }
"set"             { return FZ_SET; }
"show_cond"       { return FZ_SHOWCOND; }
"show"            { return FZ_SHOW; }
"solve"           { return FZ_SOLVE; }
"string"          { return FZ_STRING; }
"test"            { return FZ_TEST; }
"then"            { return FZ_THEN; }
"tuple"           { return FZ_TUPLE; }
"type"            { return FZ_TYPE; }
"var"             { yylval->bValue = true; return FZ_VAR; }
"variant_record"  { return FZ_VARIANT_RECORD; }
"where"           { return FZ_WHERE; }
[A-Za-z][A-Za-z0-9_]* { yylval->sValue = strdup(yytext); return FZ_ID; }
_[_]*[A-Za-z][A-Za-z0-9_]* { yylval->sValue = strdup(yytext); return FZ_U_ID; }
\"[^"\n]*\"       {
                    yylval->sValue = strdup(yytext+1);
                    yylval->sValue[strlen(yytext)-2] = 0;
                    return FZ_STRING_LIT; }
.                 { orfz_error("Unknown character"); }
%%
int yy_input_proc(char* buf, int size, yyscan_t yyscanner) {
  operations_research::ParserState* parm =
    static_cast<operations_research::ParserState*>(yyget_extra(yyscanner));
  return parm->FillBuffer(buf, size);
  // work around warning that yyunput is unused
  yyunput (0,buf,yyscanner);
}
