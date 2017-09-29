// Renames all yy to orfz_ in public functions.
%name-prefix "orfz_"

// List the parameter of the lexer.
%lex-param {void* scanner}

// Explicit list of the parameters of the orfz_parse() method (this also adds
// them to orfz_error(), in which we only need the scanner). Note that the
// parameter of orfz_lex() is defined below (see YYLEX_PARAM).
%parse-param {operations_research::fz::ParserContext* context}
%parse-param {operations_research::fz::Model* model}
%parse-param {bool* ok}
%parse-param {void* scanner}

// Specify a reentrant parser. (or-tools needs the api.pure declaration)
// TODO(lperron): Implement some MOE like modification.
//%define api.pure full
%pure-parser

// Code to be exported in parser.tab.hh
%code requires {
#if !defined(OR_TOOLS_FLATZINC_FLATZINC_TAB_HH_)
#define OR_TOOLS_FLATZINC_FLATZINC_TAB_HH_
#include "ortools/base/strutil.h"
#include "ortools/flatzinc/parser_util.h"

// Tells flex to use the LexerInfo class to communicate with the bison parser.
typedef operations_research::fz::LexerInfo YYSTYPE;

// Defines the parameter to the orfz_lex() call from the orfz_parse() method.
#define YYLEX_PARAM scanner

#endif  // OR_TOOLS_FLATZINC_FLATZINC_TAB_HH_
}  // code requires

// Code in the implementation file.
%code {
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
}  // code

%error-verbose

// Type declarations.

// The lexer, defined in the ./flazinc.lex file, does the low-level parsing
// of std::string tokens and converts each of them them into a YACC token. A YACC
// token has a type (VAR, IVALUE, const_literal) and optionally a value,
// stored in a token-specific field of a LexerInfo instance dedicated to this
// token. Eg. "26" is converted into an YACC token of type IVALUE, with value
// 26.
// YACC tokens that come from the lexer are called "terminal"; as opposed to
// tokens that are defined within this .yy file as combinations of other tokens.
// Single characters like ';' are also accepted as YACC tokens.
// Multi-characters constants (eg. "::") aren't, which is why we need to
// define them here.
//
// Here are the terminal, valueless tokens. See the .lex file to see where
// they come from.
%token ARRAY BOOL CONSTRAINT FLOAT INT MAXIMIZE MINIMIZE OF
%token PREDICATE SATISFY SET SOLVE VAR DOTDOT COLONCOLON
// Here are the terminal, value-carrying tokens, preceded by the name of the
// LexerInfo field in which their value is stored (eg. the value of a IVALUE
//  token is stored in LexerInfo::integer_value).
%token <integer_value> IVALUE
%token <string_value> SVALUE IDENTIFIER
%token <double_value> DVALUE
// Here are the non-terminal, value-carrying rules.
// Again they are preceded by the name of the LexerInfo field in which
// their value is stored. You need the %type declaration to associate a field
// with a non-terminal rule.
%type <integer_value> integer
%type <domain> domain const_literal int_domain float_domain set_domain
%type <domains> const_literals
%type <integers> integers
%type <double_value> float
%type <doubles> floats
%type <args> arguments
%type <arg> argument
%type <var_or_value> optional_var_or_value var_or_value
%type <var_or_value_array> optional_var_or_value_array var_or_value_array
%type <annotation> annotation
%type <annotations> annotations annotation_arguments
%%

//---------------------------------------------------------------------------
// Model top-level
//---------------------------------------------------------------------------

model: predicates variable_or_constant_declarations constraints solve ';'

//---------------------------------------------------------------------------
// Parsing predicates (We just ignore them, but we need to parse them anyway)
//---------------------------------------------------------------------------

predicates:
  predicates predicate ';'
// We help the parser terminate in case it fails to parse something,
// by injecting an "error" clause in the first top-level section
// (i.e. predicates). This arcane token does that.
| predicates error ';' { yyerrok; }  // Minimal error recovery.
| /* empty */

// TODO(lperron): Implement better error recovery.

predicate:
  PREDICATE IDENTIFIER '(' predicate_arguments ')'

predicate_arguments:  // Cannot be empty.
  predicate_argument ',' predicate_arguments
| predicate_argument

predicate_argument:
  domain ':' IDENTIFIER
| VAR domain ':' IDENTIFIER
| ARRAY '[' predicate_array_argument ']' OF domain ':' IDENTIFIER
| ARRAY '[' predicate_array_argument ']' OF VAR domain ':' IDENTIFIER

predicate_array_argument:
  predicate_ints
| IVALUE DOTDOT IVALUE

predicate_ints:
  INT ',' predicate_ints
| INT

//---------------------------------------------------------------------------
// Parsing variables or constants (named objects).
//---------------------------------------------------------------------------

variable_or_constant_declarations:
  variable_or_constant_declarations variable_or_constant_declaration ';'
| /* empty */

variable_or_constant_declaration:
  domain ':' IDENTIFIER annotations '=' const_literal {
  // Declaration of a (named) constant: we simply register it in the
  // parser's context, and don't store it in the model.
  const Domain& domain = $1;
  const std::string& identifier = $3;
  const Domain& assignment = $6;
  std::vector<Annotation>* const annotations = $4;


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
| ARRAY '[' IVALUE DOTDOT IVALUE ']' OF int_domain ':' IDENTIFIER
    annotations '=' '[' integers ']' {
  std::vector<Annotation>* const annotations = $11;
  // Declaration of a (named) constant array. See rule right above.
  CHECK_EQ($3, 1) << "Only [1..n] array are supported here.";
  const int64 num_constants = $5;
  const std::string& identifier = $10;
  const std::vector<int64>* const assignments = $14;
  CHECK(assignments != nullptr);
  CHECK_EQ(num_constants, assignments->size());
  // TODO(lperron): CHECK all values within domain.
  context->integer_array_map[identifier] = *assignments;
  delete assignments;
  delete annotations;
}
| ARRAY '[' IVALUE DOTDOT IVALUE ']' OF int_domain ':' IDENTIFIER
    annotations '=' '[' ']' {
  std::vector<Annotation>* const annotations = $11;
  // Declaration of a (named) constant array. See rule right above.
  CHECK_EQ($3, 1) << "Only [1..n] array are supported here.";
  const int64 num_constants = $5;
  CHECK_EQ(num_constants, 0) << "Empty arrays should have a size of 0";
  const std::string& identifier = $10;
  context->integer_array_map[identifier] = std::vector<int64>();
  delete annotations;
}
| ARRAY '[' IVALUE DOTDOT IVALUE ']' OF float_domain ':' IDENTIFIER
    annotations '=' '[' floats ']' {
  std::vector<Annotation>* const annotations = $11;
  // Declaration of a (named) constant array. See rule right above.
  CHECK_EQ($3, 1) << "Only [1..n] array are supported here.";
  const int64 num_constants = $5;
  const std::string& identifier = $10;
  const std::vector<double>* const assignments = $14;
  CHECK(assignments != nullptr);
  CHECK_EQ(num_constants, assignments->size());
  // TODO(lperron): CHECK all values within domain.
  context->float_array_map[identifier] = *assignments;
  delete assignments;
  delete annotations;
}
| ARRAY '[' IVALUE DOTDOT IVALUE ']' OF float_domain ':' IDENTIFIER
    annotations '=' '[' ']' {
  std::vector<Annotation>* const annotations = $11;
  // Declaration of a (named) constant array. See rule right above.
  CHECK_EQ($3, 1) << "Only [1..n] array are supported here.";
  const int64 num_constants = $5;
  CHECK_EQ(num_constants, 0) << "Empty arrays should have a size of 0";
  const std::string& identifier = $10;
  context->float_array_map[identifier] = std::vector<double>();
  delete annotations;
}
| ARRAY '[' IVALUE DOTDOT IVALUE ']' OF set_domain ':' IDENTIFIER
    annotations '=' '[' const_literals ']' {
  // Declaration of a (named) constant array: See rule above.
  CHECK_EQ($3, 1) << "Only [1..n] array are supported here.";
  const int64 num_constants = $5;
  const Domain& domain = $8;
  const std::string& identifier = $10;
  const std::vector<Domain>* const assignments = $14;
  const std::vector<Annotation>* const annotations = $11;
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
| VAR domain ':' IDENTIFIER annotations optional_var_or_value {
  // Declaration of a variable. If it's unassigned or assigned to a
  // constant, we'll create a new var stored in the model. If it's
  // assigned to another variable x then we simply adjust that
  // existing variable x according to the current (re-)declaration.
  const Domain& domain = $2;
  const std::string& identifier = $4;
  std::vector<Annotation>* const annotations = $5;
  const VariableRefOrValue& assignment = $6;
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
| ARRAY '[' IVALUE DOTDOT IVALUE ']' OF VAR domain ':' IDENTIFIER
    annotations optional_var_or_value_array {
  // Declaration of a "variable array": these is exactly like N simple
  // variable declarations, where the identifier for declaration #i is
  // IDENTIFIER[i] (1-based index).
  CHECK_EQ($3, 1);
  const int64 num_vars = $5;
  const Domain& domain = $9;
  const std::string& identifier = $11;
  std::vector<Annotation>* const annotations = $12;
  VariableRefOrValueArray* const assignments = $13;
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

optional_var_or_value:
  '=' var_or_value { $$ = $2; }
| /*empty*/ { $$ = VariableRefOrValue::Undefined(); }

optional_var_or_value_array:
  '=' '[' var_or_value_array ']' { $$ = $3; }
| '=' '[' ']' { $$ = nullptr; }
| /*empty*/ { $$ = nullptr; }

var_or_value_array:  // Cannot be empty.
  var_or_value_array ',' var_or_value {
  $$ = $1;
  $$->PushBack($3);
}
| var_or_value {
  $$ = new VariableRefOrValueArray();
  $$->PushBack($1);
}

var_or_value:
  IVALUE { $$ = VariableRefOrValue::Value($1); }  // An integer value.
| IDENTIFIER {
  // A reference to an existing integer constant or variable.
  const std::string& id = $1;
  if (ContainsKey(context->integer_map, id)) {
    $$ = VariableRefOrValue::Value(FindOrDie(context->integer_map, id));
  } else if (ContainsKey(context->variable_map, id)) {
    $$ = VariableRefOrValue::VariableRef(FindOrDie(context->variable_map, id));
  } else {
    LOG(ERROR) << "Unknown symbol " << id;
    $$ = VariableRefOrValue::Undefined();
    *ok = false;
  }
}
| IDENTIFIER '[' IVALUE ']' {
  // A given element of an existing constant array or variable array.
  const std::string& id = $1;
  const int64 value = $3;
  if (ContainsKey(context->integer_array_map, id)) {
    $$ = VariableRefOrValue::Value(
        Lookup(FindOrDie(context->integer_array_map, id), value));
  } else if (ContainsKey(context->variable_array_map, id)) {
    $$ = VariableRefOrValue::VariableRef(
        Lookup(FindOrDie(context->variable_array_map, id), value));
  } else {
    LOG(ERROR) << "Unknown symbol " << id;
    $$ = VariableRefOrValue::Undefined();
    *ok = false;
  }
}

int_domain:
  BOOL { $$ = Domain::Boolean(); }
| INT { $$ = Domain::AllInt64(); }
| IVALUE DOTDOT IVALUE { $$ = Domain::Interval($1, $3); }
| '{' integers '}' {
  CHECK($2 != nullptr);
  $$ = Domain::IntegerList(std::move(*$2));
  delete $2;
}

set_domain:
  SET OF BOOL { $$ = Domain::SetOfBoolean(); }
| SET OF INT { $$ = Domain::SetOfAllInt64(); }
| SET OF IVALUE DOTDOT IVALUE { $$ = Domain::SetOfInterval($3, $5); }
| SET OF '{' integers '}' {
  CHECK($4 != nullptr);
  $$ = Domain::SetOfIntegerList(std::move(*$4));
  delete $4;
}

float_domain:
  FLOAT { $$ = Domain::AllInt64(); }  // TODO(lperron): implement floats.
| DVALUE DOTDOT DVALUE {
  const int64 lb = ConvertAsIntegerOrDie($1);
  const int64 ub = ConvertAsIntegerOrDie($3);
  $$ = Domain::Interval(lb, ub);
}  // TODO(lperron): floats.

domain:
  int_domain { $$ = $1; }
| set_domain { $$ = $1; }
| float_domain { $$ = $1; }

integers:
  integers ',' integer { $$ = $1; $$->emplace_back($3); }
| integer { $$ = new std::vector<int64>(); $$->emplace_back($1); }

integer:
  IVALUE { $$ = $1; }
| IDENTIFIER { $$ = FindOrDie(context->integer_map, $1); }
| IDENTIFIER '[' IVALUE ']' {
  $$ = Lookup(FindOrDie(context->integer_array_map, $1), $3);
}

floats:
  floats ',' float { $$ = $1; $$->emplace_back($3); }
| float { $$ = new std::vector<double>(); $$->emplace_back($1); }

float:
  DVALUE { $$ = $1; }
| IDENTIFIER { $$ = FindOrDie(context->float_map, $1); }
| IDENTIFIER '[' IVALUE ']' {
  $$ = Lookup(FindOrDie(context->float_array_map, $1), $3);
}

const_literal:
  IVALUE { $$ = Domain::IntegerValue($1); }
| IVALUE DOTDOT IVALUE { $$ = Domain::Interval($1, $3); }
| '{' integers '}' {
  CHECK($2 != nullptr);
  $$ = Domain::IntegerList(std::move(*$2));
  delete $2;
}
| '{' '}' { $$ = Domain::EmptyDomain(); }
| DVALUE {
  CHECK_EQ(std::round($1), $1);
  $$ = Domain::IntegerValue(static_cast<int64>($1));
}  // TODO(lperron): floats.
| IDENTIFIER { $$ = Domain::IntegerValue(FindOrDie(context->integer_map, $1)); }
| IDENTIFIER '[' IVALUE ']' {
  $$ = Domain::IntegerValue(
      Lookup(FindOrDie(context->integer_array_map, $1), $3));
}

const_literals:
  const_literals ',' const_literal {
  $$ = $1;
  $$->emplace_back($3);
}
| const_literal { $$ = new std::vector<Domain>(); $$->emplace_back($1); }

//---------------------------------------------------------------------------
// Parsing constraints
//---------------------------------------------------------------------------

constraints: constraints constraint ';'
| /* empty */

constraint :
  CONSTRAINT IDENTIFIER '(' arguments ')' annotations {
  const std::string& identifier = $2;
  CHECK($4 != nullptr) << "Missing argument in constraint";
  const std::vector<Argument>& arguments = *$4;
  std::vector<Annotation>* const annotations = $6;

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
  delete $4;
}

arguments:
  arguments ',' argument { $$ = $1; $$->emplace_back($3); }
| argument { $$ = new std::vector<Argument>(); $$->emplace_back($1); }

argument:
  IVALUE { $$ = Argument::IntegerValue($1); }
| DVALUE { $$ = Argument::IntegerValue(ConvertAsIntegerOrDie($1)); }
| SVALUE { $$ = Argument::VoidArgument(); }
| IVALUE DOTDOT IVALUE { $$ = Argument::Interval($1, $3); }
| '{' integers '}' {
  CHECK($2 != nullptr);
  $$ = Argument::IntegerList(std::move(*$2));
  delete $2;
}
| IDENTIFIER {
  const std::string& id = $1;
  if (ContainsKey(context->integer_map, id)) {
    $$ = Argument::IntegerValue(FindOrDie(context->integer_map, id));
  } else if (ContainsKey(context->integer_array_map, id)) {
    $$ = Argument::IntegerList(FindOrDie(context->integer_array_map, id));
  } else if (ContainsKey(context->float_map, id)) {
    const double d = FindOrDie(context->float_map, id);
    $$ = Argument::IntegerValue(ConvertAsIntegerOrDie(d));
  } else if (ContainsKey(context->float_array_map, id)) {
    const auto& double_values = FindOrDie(context->float_array_map, id);
    std::vector<int64> integer_values;
    for (const double d : double_values) {
      const int64 i = ConvertAsIntegerOrDie(d);
      integer_values.push_back(i);
    }
    $$ = Argument::IntegerList(std::move(integer_values));
  } else if (ContainsKey(context->variable_map, id)) {
    $$ = Argument::IntVarRef(FindOrDie(context->variable_map, id));
  } else if (ContainsKey(context->variable_array_map, id)) {
    $$ = Argument::IntVarRefArray(FindOrDie(context->variable_array_map, id));
  } else if (ContainsKey(context->domain_map, id)) {
    const Domain& d = FindOrDie(context->domain_map, id);
    $$ = Argument::FromDomain(d);
  } else {
    CHECK(ContainsKey(context->domain_array_map, id)) << "Unknown identifier: "
                                                      << id;
    const std::vector<Domain>& d = FindOrDie(context->domain_array_map, id);
    $$ = Argument::DomainList(d);
  }
}
| IDENTIFIER '[' IVALUE ']' {
  const std::string& id = $1;
  const int64 index = $3;
  if (ContainsKey(context->integer_array_map, id)) {
    $$ = Argument::IntegerValue(
        Lookup(FindOrDie(context->integer_array_map, id), index));
  } else if (ContainsKey(context->variable_array_map, id)) {
    $$ = Argument::IntVarRef(
        Lookup(FindOrDie(context->variable_array_map, id), index));
  } else {
    CHECK(ContainsKey(context->domain_array_map, id))
        << "Unknown identifier: " << id;
    const Domain& d =
        Lookup(FindOrDie(context->domain_array_map, id), index);
    $$ = Argument::FromDomain(d);
  }
}
| '[' var_or_value_array ']' {
  VariableRefOrValueArray* const arguments = $2;
  CHECK(arguments != nullptr);
  bool has_variables = false;
  for (int i = 0; i < arguments->Size(); ++i) {
    if (arguments->variables[i] != nullptr) {
      has_variables = true;
      break;
    }
  }
  if (has_variables) {
    $$ = Argument::IntVarRefArray(std::vector<IntegerVariable*>());
    $$.variables.reserve(arguments->Size());
    for (int i = 0; i < arguments->Size(); ++i) {
      if (arguments->variables[i] != nullptr) {
         $$.variables.emplace_back(arguments->variables[i]);
      } else {
         $$.variables.emplace_back(model->AddConstant(arguments->values[i]));
      }
    }
  } else {
    $$ = Argument::IntegerList(arguments->values);
  }
  delete arguments;
}
| '[' ']' {
  $$ = Argument::VoidArgument();
}

//---------------------------------------------------------------------------
// Parsing annotations
//---------------------------------------------------------------------------

annotations:
  annotations COLONCOLON annotation {
    $$ = $1 != nullptr ? $1 : new std::vector<Annotation>();
    $$->emplace_back($3);
  }
| /* empty */ { $$ = nullptr; }

annotation_arguments:  // Cannot be empty.
  annotation_arguments ',' annotation  { $$ = $1; $$->emplace_back($3); }
| annotation { $$ = new std::vector<Annotation>(); $$->emplace_back($1); }

annotation:
  IVALUE DOTDOT IVALUE { $$ = Annotation::Interval($1, $3); }
| IVALUE { $$ = Annotation::IntegerValue($1); }
| SVALUE { $$ = Annotation::String($1); }
| IDENTIFIER {
  const std::string& id = $1;
  if (ContainsKey(context->variable_map, id)) {
    $$ = Annotation::Variable(FindOrDie(context->variable_map, id));
  } else if (ContainsKey(context->variable_array_map, id)) {
    $$ = Annotation::VariableList(FindOrDie(context->variable_array_map, id));
  } else {
    $$ = Annotation::Identifier(id);
  }
}
| IDENTIFIER '(' annotation_arguments ')' {
  std::vector<Annotation>* const annotations = $3;
  if (annotations != nullptr) {
    $$ = Annotation::FunctionCallWithArguments($1, std::move(*annotations));
    delete annotations;
  } else {
    $$ = Annotation::FunctionCall($1);
  }
}
| IDENTIFIER '[' IVALUE ']' {
  CHECK(ContainsKey(context->variable_array_map, $1))
      << "Unknown identifier: " << $1;
  $$ = Annotation::Variable(
      Lookup(FindOrDie(context->variable_array_map, $1), $3));
}
| '[' annotation_arguments ']' {
  std::vector<Annotation>* const annotations = $2;
  if (annotations != nullptr) {
    $$ = Annotation::AnnotationList(std::move(*annotations));
    delete annotations;
  } else {
    $$ = Annotation::Empty();
  }
}

//---------------------------------------------------------------------------
// Parsing solve declaration.
//---------------------------------------------------------------------------

solve:
  SOLVE annotations SATISFY {
  if ($2 != nullptr) {
    model->Satisfy(std::move(*$2));
    delete $2;
  } else {
    model->Satisfy(std::vector<Annotation>());
  }
}
| SOLVE annotations MINIMIZE argument {
  CHECK_EQ(Argument::INT_VAR_REF, $4.type);
  if ($2 != nullptr) {
    model->Minimize($4.Var(), std::move(*$2));
    delete $2;
  } else {
    model->Minimize($4.Var(), std::vector<Annotation>());
  }
}
| SOLVE annotations MAXIMIZE argument {
  CHECK_EQ(Argument::INT_VAR_REF, $4.type);
  if ($2 != nullptr) {
    model->Maximize($4.Var(), std::move(*$2));
    delete $2;
  } else {
    model->Maximize($4.Var(), std::vector<Annotation>());
  }
}

%%
