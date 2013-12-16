/* -*- mode: C++; c-basic-offset: 2; indent-tabs-mode: nil -*-* /
/*
 *  Main authors:
 *     Guido Tack <tack@gecode.org>
 *  Modified:
 *     Laurent Perron for OR-Tools (laurent.perron@gmail.com)
 *
 *  Copyright:
 *     Guido Tack, 2007
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

%pure-parser
%parse-param {void* parm}
%name-prefix = "orfz_"
%{
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
  %}

%union {
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

%error-verbose

%token <iValue> FZ_INT_LIT FZ_BOOL_LIT
%token <dValue> FZ_FLOAT_LIT
%token <sValue> FZ_ID FZ_U_ID FZ_STRING_LIT

%token <bValue> FZ_VAR FZ_PAR

%token FZ_ANNOTATION
%token FZ_ANY
%token FZ_ARRAY
%token FZ_BOOL
%token FZ_CASE
%token FZ_COLONCOLON
%token FZ_CONSTRAINT
%token FZ_DEFAULT
%token FZ_DOTDOT
%token FZ_ELSE
%token FZ_ELSEIF
%token FZ_ENDIF
%token FZ_ENUM
%token FZ_FLOAT
%token FZ_FUNCTION
%token FZ_IF
%token FZ_INCLUDE
%token FZ_INT
%token FZ_LET
%token <bValue> FZ_MAXIMIZE
%token <bValue> FZ_MINIMIZE
%token FZ_OF
%token FZ_SATISFY
%token FZ_OUTPUT
%token FZ_PREDICATE
%token FZ_RECORD
%token FZ_SET
%token FZ_SHOW
%token FZ_SHOWCOND
%token FZ_SOLVE
%token FZ_STRING
%token FZ_TEST
%token FZ_THEN
%token FZ_TUPLE
%token FZ_TYPE
%token FZ_VARIANT_RECORD
%token FZ_WHERE

%type <sValue> var_par_id
%type <setLit> set_literal

%type <varIntSpec> int_init
%type <varBoolSpec> bool_init
%type <varSetSpec> set_init
%type <varFloatSpec> float_init
%type <oSet> int_ti_expr_tail bool_ti_expr_tail

%type <oIntVarSpecVec> vardecl_int_var_array_init
%type <oBoolVarSpecVec> vardecl_bool_var_array_init
%type <oFloatVarSpecVec> vardecl_float_var_array_init
%type <oSetVarSpecVec> vardecl_set_var_array_init
%type <varIntSpecVec> int_var_array_literal
%type <varBoolSpecVec> bool_var_array_literal
%type <varFloatSpecVec> float_var_array_literal
%type <varSetSpecVec> set_var_array_literal
%type <varIntSpecVec> int_init_list int_init_list_head
%type <varBoolSpecVec> bool_init_list bool_init_list_head
%type <varFloatSpecVec> float_init_list float_init_list_head
%type <varSetSpecVec> set_init_list set_init_list_head

%type <setValue> int_list int_list_head
%type <setValue> bool_list bool_list_head
%type <setValueList> set_literal_list set_literal_list_head
%type <floatSetValue> float_list float_list_head

%type <arg> flat_expr non_array_expr annotation_expr ann_non_array_expr
%type <oArg> non_array_expr_opt
%type <argVec> flat_expr_list non_array_expr_list non_array_expr_list_head

%type <iValue> solve_expr
%type <bValue> minmax

%type <argVec> annotations annotations_head
%type <arg> annotation annotation_list

%%

/********************************/
/* main goal and item lists     */
/********************************/

model : preddecl_items vardecl_items constraint_items solve_item ';'

preddecl_items:
/* empty */
| preddecl_items_head

preddecl_items_head:
preddecl_item ';'
| preddecl_items_head preddecl_item ';'

vardecl_items:
/* emtpy */
{ static_cast<ParserState*>(parm)->InitModel(); }
| vardecl_items_head
{ static_cast<ParserState*>(parm)->InitModel(); }

vardecl_items_head:
vardecl_item ';'
| vardecl_items_head vardecl_item ';'

constraint_items:
/* emtpy */
| constraint_items_head

constraint_items_head:
constraint_item ';'
| constraint_items_head constraint_item ';'

/********************************/
/* predicate declarations       */
/********************************/

preddecl_item:
FZ_PREDICATE FZ_ID '(' pred_arg_list ')'

pred_arg_list:
/* empty */
| pred_arg_list_head list_tail

pred_arg_list_head:
pred_arg
| pred_arg_list_head ',' pred_arg

pred_arg:
pred_arg_type ':' FZ_ID

pred_arg_type:
FZ_ARRAY '[' pred_array_init ']' FZ_OF pred_arg_simple_type
| FZ_ARRAY '[' pred_array_init ']' FZ_OF FZ_VAR pred_arg_simple_type
| FZ_VAR pred_arg_simple_type
| pred_arg_simple_type

pred_arg_simple_type:
int_ti_expr_tail
| FZ_SET FZ_OF int_ti_expr_tail
| FZ_BOOL
| FZ_FLOAT

pred_array_init:
pred_array_init_arg
| pred_array_init ',' pred_array_init_arg

pred_array_init_arg:
FZ_INT
| FZ_INT_LIT FZ_DOTDOT FZ_INT_LIT

/********************************/
/* variable declarations        */
/********************************/

var_par_id : FZ_ID | FZ_U_ID

vardecl_item:
FZ_VAR int_ti_expr_tail ':' var_par_id annotations non_array_expr_opt
{
  ParserState* const pp = static_cast<ParserState*>(parm);
  bool print = $5->hasAtom("output_var");
  bool introduced = $5->hasAtom("var_is_introduced");
  pp->int_var_map_[$4] = pp->int_variables_.size();
  if (print) {
    pp->output(std::string($4), new AstIntVar(pp->int_variables_.size()));
  }
  if ($6.defined()) {
    AstNode* arg = $6.value();
    if (arg->isInt()) {
      pp->int_variables_.push_back(
          new IntVarSpec($4, arg->getInt(), introduced));
    } else if (arg->isIntVar()) {
      pp->int_variables_.push_back(
          new IntVarSpec($4, Alias(arg->getIntVar()), introduced));
      pp->AddIntVarDomainConstraint(pp->int_variables_.size() - 1, $2.value());
    } else {
      orfz_assert(pp, false, "Invalid var int initializer.");
    }
    delete arg;
  } else {
    pp->int_variables_.push_back(new IntVarSpec($4, $2, introduced, true));
  }
  delete $5; free($4);
}
| FZ_VAR bool_ti_expr_tail ':' var_par_id annotations non_array_expr_opt
{
  ParserState* const pp = static_cast<ParserState*>(parm);
  bool print = $5->hasAtom("output_var");
  bool introduced = $5->hasAtom("var_is_introduced");
  pp->bool_var_map_[$4] = pp->bool_variables_.size();
  if (print) {
    pp->output(std::string($4), new AstBoolVar(pp->bool_variables_.size()));
  }
  if ($6.defined()) {
    AstNode* arg = $6.value();
    if (arg->isBool()) {
      pp->bool_variables_.push_back(
          new BoolVarSpec($4, arg->getBool(),introduced));
    } else if (arg->isBoolVar()) {
      pp->bool_variables_.push_back(
          new BoolVarSpec($4, Alias(arg->getBoolVar()), introduced));
    } else {
      orfz_assert(pp, false, "Invalid var bool initializer.");
    }
    if (!pp->hadError) {
      pp->AddBoolVarDomainConstraint(pp->bool_variables_.size()-1, $2.value());
    }
    delete arg;
  } else {
    pp->bool_variables_.push_back(new BoolVarSpec($4, $2, introduced, true));
  }
  delete $5; free($4);
}
| FZ_VAR float_ti_expr_tail ':' var_par_id annotations non_array_expr_opt
{ ParserState* const pp = static_cast<ParserState*>(parm);
  orfz_assert(pp, false, "Floats not supported.");
  delete $5; free($4);
}
| FZ_VAR FZ_SET FZ_OF int_ti_expr_tail ':' var_par_id annotations non_array_expr_opt
{
  ParserState* const pp = static_cast<ParserState*>(parm);
  bool print = $7->hasAtom("output_var");
  bool introduced = $7->hasAtom("var_is_introduced");
  pp->set_var_map_[$6] = pp->set_variables_.size();
  if (print) {
    pp->output(std::string($6), new AstSetVar(pp->set_variables_.size()));
  }
  if ($8.defined()) {
    AstNode* arg = $8.value();
    if (arg->isSet()) {
      pp->set_variables_.push_back(
          new SetVarSpec($6, arg->getSet(),introduced));
    } else if (arg->isSetVar()) {
      pp->set_variables_.push_back(
          new SetVarSpec($6, Alias(arg->getSetVar()),introduced));
      delete arg;
    } else {
      orfz_assert(pp, false, "Invalid var set initializer.");
      delete arg;
    }
    if (!pp->hadError) {
      pp->AddSetVarDomainConstraint(pp->set_variables_.size() - 1, $4.value());
    }
  } else {
    pp->set_variables_.push_back(new SetVarSpec($6, $4,introduced, true));
  }
  delete $7; free($6);
}
| FZ_INT ':' var_par_id annotations '=' non_array_expr
{
  ParserState* const pp = static_cast<ParserState*>(parm);
  orfz_assert(pp, $6->isInt(), "Invalid int initializer.");
  pp->int_map_[$3] = $6->getInt();
  delete $4; free($3);
}
| FZ_BOOL ':' var_par_id annotations '=' non_array_expr
{
  ParserState* const pp = static_cast<ParserState*>(parm);
  orfz_assert(pp, $6->isBool(), "Invalid bool initializer.");
  pp->bool_map_[$3] = $6->getBool();
  delete $4; free($3);
}
| FZ_SET FZ_OF FZ_INT ':' var_par_id annotations '=' non_array_expr
{
  ParserState* const pp = static_cast<ParserState*>(parm);
  orfz_assert(pp, $8->isSet(), "Invalid set initializer.");
  AstSetLit* set = $8->getSet();
  pp->set_map_[$5] = *set;
  delete set;
  delete $6; free($5);
}
| FZ_ARRAY '[' FZ_INT_LIT FZ_DOTDOT FZ_INT_LIT ']' FZ_OF FZ_VAR int_ti_expr_tail ':'
var_par_id annotations vardecl_int_var_array_init
{
  ParserState* const pp = static_cast<ParserState*>(parm);
  orfz_assert(pp, $3==1, "Arrays must start at 1");
  if (!pp->hadError) {
    bool print = $12->hasCall("output_array");
    std::vector<int64> vars($5);
    if (!pp->hadError) {
      if ($13.defined()) {
        std::vector<IntVarSpec*>* vsv = $13.value();
        orfz_assert(pp, vsv->size() == static_cast<unsigned int>($5),
                 "Initializer size does not match array dimension");
        if (!pp->hadError) {
          for (int i=0; i<$5; i++) {
            IntVarSpec* ivsv = (*vsv)[i];
            if (ivsv->alias) {
              vars[i] = ivsv->i;
              if ($9.defined()) {
                pp->AddIntVarDomainConstraint(ivsv->i,
                                              new AstSetLit(*$9.value()));
              }
            } else {
              vars[i] = pp->int_variables_.size();
              ivsv->SetName($11);
              pp->int_variables_.push_back(ivsv);
            }
          }
        }
        delete vsv;
      } else {
        if ($5 > 0) {
          for (int i = 0; i < $5 - 1; i++) {
            vars[i] = pp->int_variables_.size();
            const std::string var_name = StringPrintf("%s[%d]", $11, i + 1);
            if ($9.defined()) {
              Option<AstSetLit*> copy =
                  Option<AstSetLit*>::some($9.value()->Copy());
              pp->int_variables_.push_back(
                  new IntVarSpec(var_name, copy, false, true));
            } else {
              pp->int_variables_.push_back(
                  new IntVarSpec(var_name, $9, false, true));
            }
          }
          vars[$5 - 1] = pp->int_variables_.size();
          const std::string var_name =
              StringPrintf("%s[%" GG_LL_FORMAT "d]", $11, $5);
          pp->int_variables_.push_back(
              new IntVarSpec(var_name, $9, false, true));
        }
      }
    }
    if (print) {
      AstArray* a = new AstArray();
      a->a.push_back(ArrayOutput($12->getCall("output_array")));
      AstArray* output = new AstArray();
      for (int i=0; i<$5; i++)
        output->a.push_back(new AstIntVar(vars[i]));
      a->a.push_back(output);
      a->a.push_back(new AstString(")"));
      pp->output(std::string($11), a);
    }
    pp->int_var_array_map_[$11] = vars;
  }
  delete $12; free($11);
}
| FZ_ARRAY '[' FZ_INT_LIT FZ_DOTDOT FZ_INT_LIT ']' FZ_OF FZ_VAR bool_ti_expr_tail ':'
var_par_id annotations vardecl_bool_var_array_init
{
  ParserState* const pp = static_cast<ParserState*>(parm);
  bool print = $12->hasCall("output_array");
  orfz_assert(pp, $3==1, "Arrays must start at 1");
  if (!pp->hadError) {
    std::vector<int64> vars($5);
    if ($13.defined()) {
      std::vector<BoolVarSpec*>* vsv = $13.value();
      orfz_assert(pp, vsv->size() == static_cast<unsigned int>($5),
               "Initializer size does not match array dimension");
      if (!pp->hadError) {
        for (int i = 0; i < $5; i++) {
          BoolVarSpec* const bvsv = (*vsv)[i];
          if (bvsv->alias)
            vars[i] = bvsv->i;
          else {
            vars[i] = pp->bool_variables_.size();
            const std::string var_name = StringPrintf("%s[%d]", $11, i + 1);
            (*vsv)[i]->SetName(var_name);
            pp->bool_variables_.push_back((*vsv)[i]);
          }
          if (!pp->hadError && $9.defined()) {
            AstSetLit* const domain = new AstSetLit(*$9.value());
            pp->AddBoolVarDomainConstraint(vars[i], domain);
          }
        }
      }
      delete vsv;
    } else {
      for (int i = 0; i < $5; i++) {
        vars[i] = pp->bool_variables_.size();
        const std::string var_name = StringPrintf("%s[%d]", $11, i + 1);
        pp->bool_variables_.push_back(
            new BoolVarSpec(var_name, $9, !print, (i == $5 - 1)));
      }
    }
    if (print) {
      AstArray* a = new AstArray();
      a->a.push_back(ArrayOutput($12->getCall("output_array")));
      AstArray* output = new AstArray();
      for (int i=0; i<$5; i++)
        output->a.push_back(new AstBoolVar(vars[i]));
      a->a.push_back(output);
      a->a.push_back(new AstString(")"));
      pp->output(std::string($11), a);
    }
    pp->bool_var_array_map_[$11] = vars;
  }
  delete $12; free($11);
}
| FZ_ARRAY '[' FZ_INT_LIT FZ_DOTDOT FZ_INT_LIT ']' FZ_OF FZ_VAR float_ti_expr_tail ':'
var_par_id annotations vardecl_float_var_array_init
{
  ParserState* const pp = static_cast<ParserState*>(parm);
  orfz_assert(pp, false, "Floats not supported.");
  delete $12; free($11);
}
| FZ_ARRAY '[' FZ_INT_LIT FZ_DOTDOT FZ_INT_LIT ']' FZ_OF FZ_VAR FZ_SET FZ_OF int_ti_expr_tail ':'
var_par_id annotations vardecl_set_var_array_init
{
  ParserState* const pp = static_cast<ParserState*>(parm);
  bool print = $14->hasCall("output_array");
  orfz_assert(pp, $3==1, "Arrays must start at 1");
  if (!pp->hadError) {
    std::vector<int64> vars($5);
    if ($15.defined()) {
      std::vector<SetVarSpec*>* vsv = $15.value();
      orfz_assert(pp, vsv->size() == static_cast<unsigned int>($5),
               "Initializer size does not match array dimension");
      if (!pp->hadError) {
        for (int i=0; i<$5; i++) {
          SetVarSpec* svsv = (*vsv)[i];
          if (svsv->alias)
            vars[i] = svsv->i;
          else {
            vars[i] = pp->set_variables_.size();
            (*vsv)[i]->SetName($13);
            pp->set_variables_.push_back((*vsv)[i]);
          }
          if (!pp->hadError && $11.defined()) {
            AstSetLit* const domain = new AstSetLit(*$11.value());
            pp->AddSetVarDomainConstraint(vars[i], domain);
          }
        }
      }
      delete vsv;
    } else {
      if ($5>0) {
        std::string arrayname = "["; arrayname += $13;
        for (int i=0; i<$5-1; i++) {
          vars[i] = pp->set_variables_.size();
          pp->set_variables_.push_back(
              new SetVarSpec(arrayname, $11, !print, false));
        }
        vars[$5-1] = pp->set_variables_.size();
        pp->set_variables_.push_back(new SetVarSpec($13, $11, !print, true));
      }
    }
    if (print) {
      AstArray* a = new AstArray();
      a->a.push_back(ArrayOutput($14->getCall("output_array")));
      AstArray* output = new AstArray();
      for (int i=0; i<$5; i++)
        output->a.push_back(new AstSetVar(vars[i]));
      a->a.push_back(output);
      a->a.push_back(new AstString(")"));
      pp->output(std::string($13), a);
    }
    pp->set_var_array_map_[$13] = vars;
  }
  delete $14; free($13);
}
| FZ_ARRAY '[' FZ_INT_LIT FZ_DOTDOT FZ_INT_LIT ']' FZ_OF FZ_INT ':'
var_par_id annotations '=' '[' int_list ']'
{
  ParserState* const pp = static_cast<ParserState*>(parm);
  orfz_assert(pp, $3==1, "Arrays must start at 1");
  orfz_assert(pp, $14->size() == static_cast<unsigned int>($5),
           "Initializer size does not match array dimension");
  if (!pp->hadError)
    pp->int_value_array_map_[$10] = *$14;
  delete $14;
  free($10);
  delete $11;
}
| FZ_ARRAY '[' FZ_INT_LIT FZ_DOTDOT FZ_INT_LIT ']' FZ_OF FZ_BOOL ':'
var_par_id annotations '=' '[' bool_list ']'
{
  ParserState* const pp = static_cast<ParserState*>(parm);
  orfz_assert(pp, $3==1, "Arrays must start at 1");
  orfz_assert(pp, $14->size() == static_cast<unsigned int>($5),
           "Initializer size does not match array dimension");
  if (!pp->hadError)
    pp->bool_value_array_map_[$10] = *$14;
  delete $14;
  free($10);
  delete $11;
}
| FZ_ARRAY '[' FZ_INT_LIT FZ_DOTDOT FZ_INT_LIT ']' FZ_OF FZ_FLOAT ':'
var_par_id annotations '=' '[' float_list ']'
{
  ParserState* const pp = static_cast<ParserState*>(parm);
  orfz_assert(pp, false, "Floats not supported.");
  delete $11; free($10);
}
| FZ_ARRAY '[' FZ_INT_LIT FZ_DOTDOT FZ_INT_LIT ']' FZ_OF FZ_SET FZ_OF FZ_INT ':'
var_par_id annotations '=' '[' set_literal_list ']'
{
  ParserState* const pp = static_cast<ParserState*>(parm);
  orfz_assert(pp, $3==1, "Arrays must start at 1");
  orfz_assert(pp, $16->size() == static_cast<unsigned int>($5),
           "Initializer size does not match array dimension");
  if (!pp->hadError)
    pp->set_value_array_map_[$12] = *$16;
  delete $16;
  delete $13; free($12);
}

int_init :
FZ_INT_LIT
{
  $$ = new IntVarSpec("", $1, false);
}
| var_par_id
{
  int64 v = 0;
  ParserState* const pp = static_cast<ParserState*>(parm);
  if (Get(pp->int_var_map_, $1, v))
    $$ = new IntVarSpec("", Alias(v), false);
  else {
    LOG(ERROR) << "Error: undefined identifier " << $1
               << " in line no. " << orfz_get_lineno(pp->yyscanner);
    pp->hadError = true;
    $$ = new IntVarSpec("", 0, false); // keep things consistent
  }
  free($1);
}
| var_par_id '[' FZ_INT_LIT ']'
{
  std::vector<int64> v;
  ParserState* const pp = static_cast<ParserState*>(parm);
  if (Get(pp->int_var_array_map_, $1, v)) {
    orfz_assert(pp,static_cast<unsigned int>($3) > 0 &&
             static_cast<unsigned int>($3) <= v.size(),
             "array access out of bounds");
    if (!pp->hadError)
      $$ = new IntVarSpec($1, Alias(v[$3 - 1]),false);
    else
      $$ = new IntVarSpec($1, 0,false); // keep things consistent
  } else {
    LOG(ERROR) << "Error: undefined array identifier " << $1
               << " in line no. " << orfz_get_lineno(pp->yyscanner);
    pp->hadError = true;
    $$ = new IntVarSpec($1, 0,false); // keep things consistent
  }
  free($1);
}

int_init_list :
/* empty */
{ $$ = new std::vector<IntVarSpec*>(0); }
| int_init_list_head list_tail
{ $$ = $1; }

int_init_list_head :
int_init
{ $$ = new std::vector<IntVarSpec*>(1); (*$$)[0] = $1; }
| int_init_list_head ',' int_init
{ $$ = $1; $$->push_back($3); }

list_tail : | ','

int_var_array_literal : '[' int_init_list ']'
{ $$ = $2; }

float_init :
FZ_FLOAT_LIT
{ $$ = new FloatVarSpec("", $1,false); }
| var_par_id
{
  int64 v = 0;
  ParserState* const pp = static_cast<ParserState*>(parm);
  if (Get(pp->float_var_map_, $1, v))
    $$ = new FloatVarSpec("", Alias(v),false);
  else {
    LOG(ERROR) << "Error: undefined identifier " << $1
               << " in line no. " << orfz_get_lineno(pp->yyscanner);
    pp->hadError = true;
    $$ = new FloatVarSpec("", 0.0,false);
  }
  free($1);
}
| var_par_id '[' FZ_INT_LIT ']'
{
  std::vector<int64> v;
  ParserState* const pp = static_cast<ParserState*>(parm);
  if (Get(pp->float_var_array_map_, $1, v)) {
    orfz_assert(pp,static_cast<unsigned int>($3) > 0 &&
             static_cast<unsigned int>($3) <= v.size(),
             "array access out of bounds");
    if (!pp->hadError)
      $$ = new FloatVarSpec($1, Alias(v[$3-1]),false);
    else
      $$ = new FloatVarSpec($1, 0.0,false);
  } else {
    LOG(ERROR) << "Error: undefined array identifier " << $1
               << " in line no. " << orfz_get_lineno(pp->yyscanner);
    pp->hadError = true;
    $$ = new FloatVarSpec($1, 0.0,false);
  }
  free($1);
}

float_init_list :
/* empty */
{ $$ = new std::vector<FloatVarSpec*>(0); }
| float_init_list_head list_tail
{ $$ = $1; }

float_init_list_head :
float_init
{ $$ = new std::vector<FloatVarSpec*>(1); (*$$)[0] = $1; }
| float_init_list_head ',' float_init
{ $$ = $1; $$->push_back($3); }

float_var_array_literal :
'[' float_init_list ']'
{ $$ = $2; }

bool_init :
FZ_BOOL_LIT
{ $$ = new BoolVarSpec("", $1,false); }
| var_par_id
{
  int64 v = 0;
  ParserState* const pp = static_cast<ParserState*>(parm);
  if (Get(pp->bool_var_map_, $1, v))
    $$ = new BoolVarSpec("", Alias(v),false);
  else {
    LOG(ERROR) << "Error: undefined identifier " << $1
               << " in line no. " << orfz_get_lineno(pp->yyscanner);
    pp->hadError = true;
    $$ = new BoolVarSpec("", false,false);
  }
  free($1);
}
| var_par_id '[' FZ_INT_LIT ']'
{
  std::vector<int64> v;
  ParserState* const pp = static_cast<ParserState*>(parm);
  if (Get(pp->bool_var_array_map_, $1, v)) {
    orfz_assert(pp,static_cast<unsigned int>($3) > 0 &&
             static_cast<unsigned int>($3) <= v.size(),
             "array access out of bounds");
    if (!pp->hadError)
      $$ = new BoolVarSpec($1, Alias(v[$3-1]),false);
    else
      $$ = new BoolVarSpec($1, false,false);
  } else {
    LOG(ERROR) << "Error: undefined array identifier " << $1
               << " in line no. " << orfz_get_lineno(pp->yyscanner);
    pp->hadError = true;
    $$ = new BoolVarSpec($1, false,false);
  }
  free($1);
}

bool_init_list :
/* empty */
{ $$ = new std::vector<BoolVarSpec*>(0); }
| bool_init_list_head list_tail
{ $$ = $1; }

bool_init_list_head :
bool_init
{ $$ = new std::vector<BoolVarSpec*>(1); (*$$)[0] = $1; }
| bool_init_list_head ',' bool_init
{ $$ = $1; $$->push_back($3); }

bool_var_array_literal : '[' bool_init_list ']' { $$ = $2; }

set_init :
set_literal
{ $$ = new SetVarSpec("", $1,false); }
| var_par_id
{
  ParserState* const pp = static_cast<ParserState*>(parm);
  int64 v = 0;
  if (Get(pp->set_var_map_, $1, v)) {
    $$ = new SetVarSpec("", Alias(v),false);
  } else {
    LOG(ERROR) << "Error: undefined identifier " << $1
               << " in line no. " << orfz_get_lineno(pp->yyscanner);
    pp->hadError = true;
    $$ = new SetVarSpec("", Alias(0),false);
  }
  free($1);
}
| var_par_id '[' FZ_INT_LIT ']'
{
  std::vector<int64> v;
  ParserState* const pp = static_cast<ParserState*>(parm);
  if (Get(pp->set_var_array_map_, $1, v)) {
    orfz_assert(pp,static_cast<unsigned int>($3) > 0 &&
             static_cast<unsigned int>($3) <= v.size(),
             "array access out of bounds");
    if (!pp->hadError)
      $$ = new SetVarSpec($1, Alias(v[$3-1]),false);
    else
      $$ = new SetVarSpec($1, Alias(0),false);
  } else {
    LOG(ERROR) << "Error: undefined array identifier " << $1
               << " in line no. " << orfz_get_lineno(pp->yyscanner);
    pp->hadError = true;
    $$ = new SetVarSpec($1, Alias(0),false);
  }
  free($1);
}

set_init_list :
/* empty */
{ $$ = new std::vector<SetVarSpec*>(0); }
| set_init_list_head list_tail
{ $$ = $1; }

set_init_list_head :
set_init
{ $$ = new std::vector<SetVarSpec*>(1); (*$$)[0] = $1; }
| set_init_list_head ',' set_init
{ $$ = $1; $$->push_back($3); }

set_var_array_literal : '[' set_init_list ']'
{ $$ = $2; }

vardecl_int_var_array_init :
/* empty */
{ $$ = Option<std::vector<IntVarSpec*>*>::none(); }
| '=' int_var_array_literal
{ $$ = Option<std::vector<IntVarSpec*>*>::some($2); }

vardecl_bool_var_array_init :
/* empty */
{ $$ = Option<std::vector<BoolVarSpec*>*>::none(); }
| '=' bool_var_array_literal
{ $$ = Option<std::vector<BoolVarSpec*>*>::some($2); }

vardecl_float_var_array_init :
/* empty */
{ $$ = Option<std::vector<FloatVarSpec*>*>::none(); }
| '=' float_var_array_literal
{ $$ = Option<std::vector<FloatVarSpec*>*>::some($2); }

vardecl_set_var_array_init :
/* empty */
{ $$ = Option<std::vector<SetVarSpec*>*>::none(); }
| '=' set_var_array_literal
{ $$ = Option<std::vector<SetVarSpec*>*>::some($2); }

constraint_item :
FZ_CONSTRAINT FZ_ID '(' flat_expr_list ')' annotations
{
  ParserState* const pp = static_cast<ParserState*>(parm);
  if (!pp->hadError) {
    try {
      pp->AddConstraint($2, $4, $6);
    } catch (operations_research::FzError& e) {
      yyerror(pp, e.DebugString().c_str());
    }
  }
  free($2);
}
solve_item :
FZ_SOLVE annotations FZ_SATISFY
{
  ParserState* const pp = static_cast<ParserState*>(parm);
  if (!pp->hadError) {
    try {
      pp->model()->Satisfy($2);
      pp->AnalyseAndCreateModel();
    } catch (operations_research::FzError& e) {
      yyerror(pp, e.DebugString().c_str());
    }
  } else {
    delete $2;
  }
}
| FZ_SOLVE annotations minmax solve_expr
{
  ParserState* const pp = static_cast<ParserState*>(parm);
  if (!pp->hadError) {
    try {
      if ($3)
        pp->model()->Minimize($4,$2);
      else
        pp->model()->Maximize($4,$2);
      pp->AnalyseAndCreateModel();
    } catch (operations_research::FzError& e) {
      yyerror(pp, e.DebugString().c_str());
    }
  } else {
    delete $2;
  }
}

/********************************/
/* type-insts                   */
/********************************/

int_ti_expr_tail :
FZ_INT
{ $$ = Option<AstSetLit*>::none(); }
| '{' int_list '}'
{ $$ = Option<AstSetLit*>::some(new AstSetLit(*$2)); }
| FZ_INT_LIT FZ_DOTDOT FZ_INT_LIT
{
  $$ = Option<AstSetLit*>::some(new AstSetLit($1, $3));
}

bool_ti_expr_tail :
FZ_BOOL
{ $$ = Option<AstSetLit*>::none(); }
| '{' bool_list_head list_tail '}'
{ bool haveTrue = false;
  bool haveFalse = false;
  for (int i=$2->size(); i--;) {
    haveTrue |= ((*$2)[i] == 1);
    haveFalse |= ((*$2)[i] == 0);
  }
  delete $2;
  $$ = Option<AstSetLit*>::some(
      new AstSetLit(!haveFalse,haveTrue));
}

float_ti_expr_tail :
FZ_FLOAT
| '{' float_list_head list_tail '}'

/********************************/
/* literals                     */
/********************************/

set_literal :
'{' int_list '}'
{ $$ = new AstSetLit(*$2); }
| FZ_INT_LIT FZ_DOTDOT FZ_INT_LIT
{ $$ = new AstSetLit($1, $3); }

/* list containing only primitive literals */

int_list :
/* empty */
{ $$ = new std::vector<int64>(0); }
| int_list_head list_tail
{ $$ = $1; }

int_list_head :
FZ_INT_LIT
{ $$ = new std::vector<int64>(1); (*$$)[0] = $1; }
| int_list_head ',' FZ_INT_LIT
{ $$ = $1; $$->push_back($3); }

bool_list :
/* empty */
{ $$ = new std::vector<int64>(0); }
| bool_list_head list_tail
{ $$ = $1; }

bool_list_head :
FZ_BOOL_LIT
{ $$ = new std::vector<int64>(1); (*$$)[0] = $1; }
| bool_list_head ',' FZ_BOOL_LIT
{ $$ = $1; $$->push_back($3); }

float_list :
/* empty */
{ $$ = new std::vector<double>(0); }
| float_list_head list_tail
{ $$ = $1; }

float_list_head:
FZ_FLOAT_LIT
{ $$ = new std::vector<double>(1); (*$$)[0] = $1; }
| float_list_head ',' FZ_FLOAT_LIT
{ $$ = $1; $$->push_back($3); }

set_literal_list :
/* empty */
{ $$ = new std::vector<AstSetLit>(0); }
| set_literal_list_head list_tail
{ $$ = $1; }

set_literal_list_head :
set_literal
{ $$ = new std::vector<AstSetLit>(1); (*$$)[0] = *$1; delete $1; }
| set_literal_list_head ',' set_literal
{ $$ = $1; $$->push_back(*$3); delete $3; }

/********************************/
/* constraint expressions       */
/********************************/

flat_expr_list :
flat_expr
{ $$ = new AstArray($1); }
| flat_expr_list ',' flat_expr
{ $$ = $1; $$->append($3); }

flat_expr :
non_array_expr
{ $$ = $1; }
| '[' non_array_expr_list ']'
{ $$ = $2; }

non_array_expr_opt :
/* empty */
{ $$ = Option<AstNode*>::none(); }
| '=' non_array_expr
{ $$ = Option<AstNode*>::some($2); }

non_array_expr :
FZ_BOOL_LIT
{ $$ = new AstBoolLit($1); }
| FZ_INT_LIT
{ $$ = new AstIntLit($1); }
| FZ_FLOAT_LIT
{ $$ = new AstFloatLit($1); }
| set_literal
{ $$ = $1; }
| var_par_id /* variable, possibly array */
{
  std::vector<int64> as;
  ParserState* const pp = static_cast<ParserState*>(parm);
  if (Get(pp->int_var_array_map_, $1, as)) {
    AstArray* ia = new AstArray(as.size());
    for (int i=as.size(); i--;)
      ia->a[i] = new AstIntVar(as[i]);
    $$ = ia;
  } else if (Get(pp->bool_var_array_map_, $1, as)) {
    AstArray* ia = new AstArray(as.size());
    for (int i=as.size(); i--;)
      ia->a[i] = new AstBoolVar(as[i]);
    $$ = ia;
  } else if (Get(pp->set_var_array_map_, $1, as)) {
    AstArray* ia = new AstArray(as.size());
    for (int i=as.size(); i--;)
      ia->a[i] = new AstSetVar(as[i]);
    $$ = ia;
  } else {
    std::vector<int64> is;
    std::vector<AstSetLit> isS;
    int64 ival = 0;
    bool bval = false;
    if (Get(pp->int_value_array_map_, $1, is)) {
      AstArray* v = new AstArray(is.size());
      for (int i=is.size(); i--;)
        v->a[i] = new AstIntLit(is[i]);
      $$ = v;
    } else if (Get(pp->bool_value_array_map_, $1, is)) {
      AstArray* v = new AstArray(is.size());
      for (int i=is.size(); i--;)
        v->a[i] = new AstBoolLit(is[i]);
      $$ = v;
    } else if (Get(pp->set_value_array_map_, $1, isS)) {
      AstArray* v = new AstArray(isS.size());
      for (int i=isS.size(); i--;)
        v->a[i] = new AstSetLit(isS[i]);
      $$ = v;
    } else if (Get(pp->int_map_, $1, ival)) {
      $$ = new AstIntLit(ival);
    } else if (Get(pp->bool_map_, $1, bval)) {
      $$ = new AstBoolLit(bval);
    } else {
      $$ = pp->VarRefArg($1, false);
    }
  }
  free($1);
}
| var_par_id '[' non_array_expr ']' /* array access */
{
  ParserState* const pp = static_cast<ParserState*>(parm);
  int i = -1;
  orfz_assert(pp, $3->isInt(i), "Non-integer array index.");
  if (!pp->hadError)
    $$ = static_cast<ParserState*>(parm)->ArrayElement($1, i);
  else
    $$ = new AstIntLit(0); // keep things consistent
  free($1); delete $3;
}

non_array_expr_list :
/* empty */
{ $$ = new AstArray(0); }
| non_array_expr_list_head list_tail
{ $$ = $1; }

non_array_expr_list_head :
non_array_expr
{ $$ = new AstArray($1); }
| non_array_expr_list_head ',' non_array_expr
{ $$ = $1; $$->append($3); }

/********************************/
/* solve expressions            */
/********************************/

solve_expr:
var_par_id
{
  ParserState* const pp = static_cast<ParserState*>(parm);
  int64 value;
  if (!Get(pp->int_var_map_, $1, value)) {
    LOG(ERROR) << "Error: unknown integer variable " << $1
               << " in line no. " << orfz_get_lineno(pp->yyscanner);
    pp->hadError = true;
  }
  $$ = value;
  free($1);
}
| var_par_id '[' FZ_INT_LIT ']'
{
  std::vector<int64> tmp;
  ParserState* const pp = static_cast<ParserState*>(parm);
  if (!Get(pp->int_var_array_map_, $1, tmp)) {
    LOG(ERROR) << "Error: unknown integer variable array " << $1
               << " in line no. " << orfz_get_lineno(pp->yyscanner);
    pp->hadError = true;
  }
  if ($3 == 0 || static_cast<unsigned int>($3) > tmp.size()) {
    LOG(ERROR) << "Error: array index out of bounds for array " << $1
               << " in line no. " << orfz_get_lineno(pp->yyscanner);
    pp->hadError = true;
  } else {
    $$ = tmp[$3 - 1];
  }
  free($1);
}

minmax:
FZ_MINIMIZE
| FZ_MAXIMIZE

/********************************/
/* annotation expresions        */
/********************************/

annotations :
/* empty */
{ $$ = nullptr; }
| annotations_head
{ $$ = $1; }

annotations_head :
FZ_COLONCOLON annotation
{ $$ = new AstArray($2); }
| annotations_head FZ_COLONCOLON annotation
{ $$ = $1; $$->append($3); }

annotation :
FZ_ID '(' annotation_list ')'
{
  $$ = new AstCall($1, AstExtractSingleton($3)); free($1);
}
| annotation_expr
{ $$ = $1; }

annotation_list:
annotation
{ $$ = new AstArray($1); }
| annotation_list ',' annotation
{ $$ = $1; $$->append($3); }

annotation_expr :
ann_non_array_expr
{ $$ = $1; }
| '[' annotation_list ']'
{ $$ = $2; }

ann_non_array_expr :
FZ_BOOL_LIT
{ $$ = new AstBoolLit($1); }
| FZ_INT_LIT
{ $$ = new AstIntLit($1); }
| FZ_FLOAT_LIT
{ $$ = new AstFloatLit($1); }
| set_literal
{ $$ = $1; }
| var_par_id /* variable, possibly array */
{
  std::vector<int64> as;
  ParserState* const pp = static_cast<ParserState*>(parm);
  if (Get(pp->int_var_array_map_, $1, as)) {
    AstArray* const ia = new AstArray(as.size());
    for (int i = as.size(); i--;) {
      ia->a[i] = new AstIntVar(as[i]);
    }
    $$ = ia;
  } else if (Get(pp->bool_var_array_map_, $1, as)) {
    AstArray* const ia = new AstArray(as.size());
    for (int i=as.size(); i--;) {
      ia->a[i] = new AstBoolVar(as[i]);
    }
    $$ = ia;
  } else if (Get(pp->set_var_array_map_, $1, as)) {
    AstArray* const ia = new AstArray(as.size());
    for (int i = as.size(); i--;) {
      ia->a[i] = new AstSetVar(as[i]);
    }
    $$ = ia;
  } else {
    std::vector<int64> is;
    int64 ival = 0;
    bool bval = false;
    if (Get(pp->int_value_array_map_, $1, is)) {
      AstArray* const v = new AstArray(is.size());
      for (int i = is.size(); i--;) {
        v->a[i] = new AstIntLit(is[i]);
      }
      $$ = v;
    } else if (Get(pp->bool_value_array_map_, $1, is)) {
      AstArray* const v = new AstArray(is.size());
      for (int i = is.size(); i--;) {
        v->a[i] = new AstBoolLit(is[i]);
      }
      $$ = v;
    } else if (Get(pp->int_map_, $1, ival)) {
      $$ = new AstIntLit(ival);
    } else if (Get(pp->bool_map_, $1, bval)) {
      $$ = new AstBoolLit(bval);
    } else {
      $$ = pp->VarRefArg($1, true);
    }
  }
  free($1);
}
| var_par_id '[' ann_non_array_expr ']' /* array access */
{
  ParserState* const pp = static_cast<ParserState*>(parm);
  int i = -1;
  orfz_assert(pp, $3->isInt(i), "Non-integer array index.");
  if (!pp->hadError)
    $$ = pp->ArrayElement($1, i);
  else
    $$ = new AstIntLit(0); // keep things consistent
  free($1); delete $3;
}
| FZ_STRING_LIT
{
  $$ = new AstString($1);
  free($1);
}
