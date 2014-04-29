// Specify a reentrant parser.
//%pure-parser

// Renames all yy to orfz_ in public functions.
%name-prefix "orfz_"

// List the parameter of the lexer.
%lex-param {void* scanner}

// Explicit list of the parameters of the orfz_parse() method (this also adds
// them to orfz_error(), in which we only need the scanner). Note that the
// parameter of orfz_lex() is defined below (see YYLEX_PARAM).
%parse-param {operations_research::FzParserContext* context}
%parse-param {operations_research::FzModel* model}
%parse-param {bool* ok}
%parse-param {void* scanner}

%define api.pure full

// Code to be exported in parser.tab.hh
%code requires {
#if !defined(OR_TOOLS_FLATZINC_FLATZINC_TAB_HH_)
#define OR_TOOLS_FLATZINC_FLATZINC_TAB_HH_
#include "base/map_util.h"
#include "flatzinc2/model.h"

namespace operations_research {
// This is the context used during parsing.
struct FzParserContext {
  hash_map<std::string, int64> integer_map;
  hash_map<std::string, std::vector<int64> > integer_array_map;
  hash_map<std::string, FzIntegerVariable*> variable_map;
  hash_map<std::string, std::vector<FzIntegerVariable*> > variable_array_map;
  hash_map<std::string, FzDomain> domain_map;
  hash_map<std::string, std::vector<FzDomain> > domain_array_map;
};

// An optional reference to a variable, or an integer value, used in
// assignments during the declaration of a variable, or a variable
// array.
struct VariableRefOrValue {
  static VariableRefOrValue Undefined() {
    VariableRefOrValue result;
    result.variable = nullptr;
    result.value = 0;
    result.defined = false;
    return result;
  }
  static VariableRefOrValue VariableRef(FzIntegerVariable* var) {
    VariableRefOrValue result;
    result.variable = var;
    result.value = 0;
    result.defined = true;
    return result;
  }
  static VariableRefOrValue Value(int64 value) {
    VariableRefOrValue result;
    result.variable = nullptr;
    result.value = value;
    result.defined = true;
    return result;
  }

  FzIntegerVariable* variable;
  int64 value;
  bool defined;
};

// Class needed to pass information from the lexer to the parser.
struct LexerInfo {
  int64 integer_value;
  double double_value;
  std::string string_value;
  FzDomain domain;
  std::vector<FzDomain> domains;
  std::vector<int64> integers;
  FzArgument arg;
  std::vector<FzArgument> args;
  FzAnnotation annotation;
  std::vector<FzAnnotation> annotations;
  VariableRefOrValue var_or_value;
  std::vector<VariableRefOrValue> var_or_value_array;
};
}  // namespace operations_research

// Tells flex to use the LexerInfo class to communicate with the bison parser.
typedef operations_research::LexerInfo YYSTYPE;

#endif  // OR_TOOLS_FLATZINC_FLATZINC_TAB_HH_

// Defines the parameter to the orfz_lex() call from the orfz_parse() method.
#define YYLEX_PARAM scanner

}  // code requires

// Code in the implementation file.
%code {
#include <string>

#include "base/integral_types.h"
#include "base/logging.h"
#include "base/stl_util.h"
#include "flatzinc2/parser.tab.hh"
#include "flatzinc2/model.h"
#include "util/string_array.h"

extern int orfz_lex(YYSTYPE*, void* scanner);
extern int orfz_get_lineno (void* scanner);
extern int orfz_debug;

using namespace operations_research;

void orfz_error(FzParserContext* context, FzModel* model, bool* ok,
                void* scanner, const char* str) {
  LOG(ERROR) << "Error: " << str << " in line no. " << orfz_get_lineno(scanner);
  *ok = false;
}

namespace operations_research {
// Whether the given list of annotations contains the given identifier
// (or function call).
bool ContainsId(const std::vector<FzAnnotation>& annotations, const std::string& id) {
  for (int i = 0; i < annotations.size(); ++i) {
    if ((annotations[i].type == FzAnnotation::IDENTIFIER ||
         annotations[i].type == FzAnnotation::FUNCTION_CALL) &&
        annotations[i].id == id) {
      return true;
    }
  }
  return false;
}

bool AreAllSingleton(const std::vector<FzDomain>& domains) {
  for (int i = 0; i < domains.size(); ++i) {
    if (!domains[i].IsSingleton()) {
      return false;
    }
  }
  return true;
}

// Array in flatzinc are 1 based. We use this trivial wrapper for all flatzinc
// arrays.
template<class T> const T& FzLookup(const std::vector<T>& v, int index) {
  // TODO(lperron): replace this by a macro for better logging.
  CHECK_GE(index, 1);
  CHECK_LE(index, v.size());
  return v[index - 1];
}
}  // namespace operations_research
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
  const FzDomain& domain = $1;
  const std::string& identifier = $3;
  const FzDomain& assignment = $6;

  if (!assignment.IsSingleton()) {
    // TODO(lperron): Check that the assignment is included in the domain.
    context->domain_map[identifier] = assignment;
  } else {
    const int64 value = assignment.values.front();
    CHECK(domain.Contains(value));
    context->integer_map[identifier] = value;
  }
}
| ARRAY '[' IVALUE DOTDOT IVALUE ']' OF int_domain ':' IDENTIFIER
    annotations '=' '[' integers ']' {
  // Declaration of a (named) constant array. See rule right above.
  CHECK_EQ($3, 1) << "Only [1..n] array are supported here.";
  const int64 num_constants = $5;
  const std::string& identifier = $10;
  const std::vector<int64>& assignments = $14;
  CHECK_EQ(num_constants, assignments.size());
  // TODO(lperron): CHECK all values within domain.
  context->integer_array_map[identifier] = assignments;
}
| ARRAY '[' IVALUE DOTDOT IVALUE ']' OF set_domain ':' IDENTIFIER
    annotations '=' '[' const_literals ']' {
  // Declaration of a (named) constant array: See rule above.
  CHECK_EQ($3, 1) << "Only [1..n] array are supported here.";
  const int64 num_constants = $5;
  const FzDomain& domain = $8;
  const std::string& identifier = $10;
  const std::vector<FzDomain>& assignments = $14;
  CHECK_EQ(num_constants, assignments.size());

  if (!AreAllSingleton(assignments)) {
    context->domain_array_map[identifier] = assignments;
    // TODO(lperron): check that all assignments are included in the domain.
  } else {
    std::vector<int64> values(num_constants);
    for (int i = 0; i < num_constants; ++i) {
      values[i] = assignments[i].values.front();
      CHECK(domain.Contains(values[i]));
    }
    context->integer_array_map[identifier] = values;
  }
}
| VAR domain ':' IDENTIFIER annotations optional_var_or_value {
  // Declaration of a variable. If it's unassigned or assigned to a
  // constant, we'll create a new var stored in the model. If it's
  // assigned to another variable x then we simply adjust that
  // existing variable x according to the current (re-)declaration.
  const FzDomain& domain = $2;
  const std::string& identifier = $4;
  const std::vector<FzAnnotation>& annotations = $5;
  const VariableRefOrValue& assignment = $6;
  const bool introduced = ContainsId(annotations, "var_is_introduced");
  FzIntegerVariable* var = nullptr;
  if (!assignment.defined) {
    var = model->AddVariable(identifier, domain, introduced);
  } else if (assignment.variable == nullptr) {  // just an integer constant.
    CHECK(domain.Contains(assignment.value));
    var = model->AddVariable(
        identifier, FzDomain::Singleton(assignment.value), introduced);
  } else {  // a variable.
    var = assignment.variable;
    var->Merge(identifier, domain, nullptr, introduced);
  }

  // We also register the variable in the parser's context, and add some
  // output to the model if needed.
  context->variable_map[identifier] = var;
  if (ContainsId(annotations, "output_var")) {
    model->AddOutput(FzOnSolutionOutput::SingleVariable(identifier, var));
  }
}
| ARRAY '[' IVALUE DOTDOT IVALUE ']' OF VAR domain ':' IDENTIFIER
    annotations optional_var_or_value_array {
  // Declaration of a "variable array": these is exactly like N simple
  // variable declarations, where the identifier for declaration #i is
  // IDENTIFIER[i] (1-based index).
  CHECK_EQ($3, 1);
  const int64 num_vars = $5;
  const FzDomain& domain = $9;
  const std::string& identifier = $11;
  const std::vector<FzAnnotation>& annotations = $12;
  const std::vector<VariableRefOrValue>& assignments = $13;
  CHECK(assignments.empty() || assignments.size() == num_vars);
  const bool introduced = ContainsId(annotations, "introduced");

  std::vector<FzIntegerVariable*> vars(num_vars, nullptr);

  for (int i = 0; i < num_vars; ++i) {
    const std::string var_name = StringPrintf("%s[%d]", identifier.c_str(), i + 1);
    if (assignments.empty()) {
      vars[i] = model->AddVariable(var_name, domain, introduced);
    } else if (assignments[i].variable == nullptr) {
      // Assigned to an integer constant.
      CHECK(domain.Contains(assignments[i].value));
      vars[i] = model->AddVariable(
          var_name, FzDomain::Singleton(assignments[i].value), introduced);
    } else {
      CHECK(assignments[i].variable != nullptr);
      vars[i] = assignments[i].variable;
      vars[i]->Merge(var_name, domain, nullptr, introduced);
    }
  }

  // Register the variable array on the context.
  context->variable_array_map[identifier] = vars;

  // We parse the annotations to build an output object if
  // needed. It's a bit more convoluted than the simple variable
  // output.
  for (int i = 0; i < annotations.size(); ++i) {
    const FzAnnotation& ann = annotations[i];
    if (ann.IsFunctionCallWithIdentifier("output_array")) {
      // We have found an output annotation.
      CHECK_EQ(1, ann.annotations.size());
      CHECK_EQ(FzAnnotation::ANNOTATION_LIST, ann.annotations.back().type);
      const FzAnnotation& list = ann.annotations.back();
      // Let's build the vector of bounds.
      std::vector<FzOnSolutionOutput::Bounds> bounds;
      for (int a = 0; a < list.annotations.size(); ++a) {
        const FzAnnotation& bound = list.annotations[a];
        CHECK_EQ(FzAnnotation::INTERVAL, bound.type);
        bounds.push_back(
            FzOnSolutionOutput::Bounds(bound.interval_min, bound.interval_max));
      }
      // We add the output information.
      model->AddOutput(
          FzOnSolutionOutput::MultiDimensionalArray(identifier, bounds, vars));
    }
  }
}

optional_var_or_value:
  '=' var_or_value { $$ = $2; }
| /*empty*/ { $$ = VariableRefOrValue::Undefined(); }

optional_var_or_value_array:
  '=' '[' var_or_value_array ']' { $$.clear(); $$.swap($3); }
| /*empty*/ { $$.clear(); }

var_or_value_array:  // Cannot be empty.
  var_or_value_array ',' var_or_value {
    $$.clear();
    $$.swap($1);
    $$.push_back($3);
 }
| var_or_value { $$.clear(); $$.push_back($1); }

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
        FzLookup(FindOrDie(context->integer_array_map, id), value));
  } else if (ContainsKey(context->variable_array_map, id)) {
    $$ = VariableRefOrValue::VariableRef(
        FzLookup(FindOrDie(context->variable_array_map, id), value));
  } else {
    LOG(ERROR) << "Unknown symbol " << id;
    $$ = VariableRefOrValue::Undefined();
    *ok = false;
  }
}
int_domain:
  BOOL { $$ = FzDomain::Interval(0, 1); }
| INT { $$ = FzDomain::AllInt64(); }
| IVALUE DOTDOT IVALUE { $$ = FzDomain::Interval($1, $3); }
| '{' integers '}' { $$ = FzDomain::IntegerList($2); }

set_domain:
  SET OF BOOL { $$ = FzDomain::Interval(0, 1); }
| SET OF INT { $$ = FzDomain::AllInt64(); }
| SET OF IVALUE DOTDOT IVALUE { $$ = FzDomain::Interval($3, $5); }
| SET OF '{' integers '}' { $$ = FzDomain::IntegerList($4); }

float_domain:
  FLOAT { $$ = FzDomain::AllInt64(); }  // TODO(lperron): implement floats.
| DVALUE DOTDOT DVALUE { $$ = FzDomain::AllInt64(); }  // TODO(lperron): floats.


domain:
  int_domain { $$ = $1; }
| set_domain { $$ = $1; }
| float_domain { $$ = $1; }

integers:
  integers ',' integer { $$.clear(); $$.swap($1); $$.push_back($3); }
| integer { $$.clear(); $$.push_back($1); }

integer:
  IVALUE { $$ = $1; }
| IDENTIFIER { $$ = FindOrDie(context->integer_map, $1); }
| IDENTIFIER '[' IVALUE ']' {
  $$ = FzLookup(FindOrDie(context->integer_array_map, $1), $3);
}

const_literal:
  IVALUE { $$ = FzDomain::Singleton($1); }
| IVALUE DOTDOT IVALUE { $$ = FzDomain::Interval($1, $3); }
| '{' integers '}' { $$ = FzDomain::IntegerList($2); }
| DVALUE { $$ = FzDomain::AllInt64(); }  // TODO(lperron): floats.
| IDENTIFIER { $$ = FzDomain::Singleton(FindOrDie(context->integer_map, $1)); }
| IDENTIFIER '[' IVALUE ']' {
  $$ = FzDomain::Singleton(
      FzLookup(FindOrDie(context->integer_array_map, $1), $3));
}

const_literals:
  const_literals ',' const_literal {
    $$.clear();
    $$.swap($1);
    $$.push_back($3);
}
| const_literal { $$.clear(); $$.push_back($1); }

//---------------------------------------------------------------------------
// Parsing constraints
//---------------------------------------------------------------------------

constraints: constraints constraint ';'
| /* empty */

constraint :
  CONSTRAINT IDENTIFIER '(' arguments ')' annotations {
  const std::string& identifier = $2;
  const std::vector<FzArgument>& arguments = $4;
  const std::vector<FzAnnotation>& annotations = $6;

  // Does the constraint has a defines_var annotation?
  FzIntegerVariable* defines_var = nullptr;
  for (int i = 0; i < annotations.size(); ++i) {
    const FzAnnotation& ann = annotations[i];
    if (ann.IsFunctionCallWithIdentifier("defines_var")) {
      CHECK_EQ(1, ann.annotations.size());
      CHECK_EQ(FzAnnotation::INT_VAR_REF, ann.annotations.back().type);
      defines_var = ann.annotations.back().variable;
      break;
    }
  }

  model->AddConstraint(identifier, arguments,
                       ContainsId(annotations, "domain"), defines_var);
}

arguments:
  arguments ',' argument { $$.clear(); $$.swap($1); $$.push_back($3); }
| argument { $$.clear(); $$.push_back($1); }

argument:
  IVALUE { $$ = FzArgument::IntegerValue($1); }
| DVALUE { $$ = FzArgument::VoidArgument(); }
| SVALUE { $$ = FzArgument::VoidArgument(); }
| IVALUE DOTDOT IVALUE { $$ = FzArgument::Domain(FzDomain::Interval($1, $3)); }
| '{' integers '}' { $$ = FzArgument::Domain(FzDomain::IntegerList($2)); }
| IDENTIFIER {
  const std::string& id = $1;
  if (ContainsKey(context->integer_map, id)) {
    $$ = FzArgument::IntegerValue(FindOrDie(context->integer_map, id));
  } else if (ContainsKey(context->integer_array_map, id)) {
    $$ = FzArgument::Domain(
        FzDomain::IntegerList(FindOrDie(context->integer_array_map, id)));
  } else if (ContainsKey(context->variable_map, id)) {
    $$ = FzArgument::IntVarRef(FindOrDie(context->variable_map, id));
  } else if (ContainsKey(context->variable_array_map, id)) {
    $$ = FzArgument::IntVarRefArray(FindOrDie(context->variable_array_map, id));
  } else {
    CHECK(ContainsKey(context->domain_map, id)) << "Unknown identifier: " << id;
    $$ = FzArgument::Domain(FindOrDie(context->domain_map, id));
  }
}
| IDENTIFIER '[' IVALUE ']' {
  const std::string& id = $1;
  const int64 index = $3;
  if (ContainsKey(context->integer_array_map, id)) {
    $$ = FzArgument::Domain(FzDomain::Singleton(
        FzLookup(FindOrDie(context->integer_array_map, id), index)));
  } else if (ContainsKey(context->variable_array_map, id)) {
    $$ = FzArgument::IntVarRef(
        FzLookup(FindOrDie(context->variable_array_map, id), index));
  } else {
    CHECK(ContainsKey(context->domain_array_map, id))
        << "Unknown identifier: " << id;
    $$ = FzArgument::Domain(
        FzLookup(FindOrDie(context->domain_array_map, id), index));
  }
}
| '[' arguments ']' {
  const std::vector<FzArgument>& arguments = $2;
  // Infer the type of the array from a quick scan of its
  // elements. All the elements should have the same type anyway,
  // except INT_VALUE which can be mixed with INT_VAR_REF.
  FzArgument::Type type = FzArgument::INT_VALUE;
  for (int i = 0; i < arguments.size(); ++i) {
    if (arguments[i].type != FzArgument::INT_VALUE) {
      type = arguments[i].type;
      break;
    }
  }
  switch (type) {
    case FzArgument::INT_VALUE: {
      std::vector<int64> values(arguments.size());
      for (int i = 0; i < arguments.size(); ++i) {
        CHECK_EQ(FzArgument::INT_VALUE, arguments[i].type);
        values[i] = arguments[i].integer_value;
      }
      $$ = FzArgument::Domain(FzDomain::IntegerList(values));
      break;
    }
    case FzArgument::INT_VAR_REF: {
      std::vector<FzIntegerVariable*> vars(arguments.size());
      for (int i = 0; i < arguments.size(); ++i) {
        if (arguments[i].type == FzArgument::INT_VAR_REF) {
           CHECK(arguments[i].variable != nullptr);
           vars[i] = arguments[i].variable;
        } else if (FzArgument::INT_VALUE == arguments[i].type) {
           vars[i] = FzIntegerVariable::Constant(arguments[i].integer_value);
        } else {
          LOG(FATAL) << "Unsupported parameter type: " << type;
        }
      }
      $$ = FzArgument::IntVarRefArray(vars);
      break;
    }
    default: {
      LOG(FATAL) << "Unsupported array type " << type << ". Array: "
                 << JoinDebugString(arguments, ", ");
    }
  }
}

//---------------------------------------------------------------------------
// Parsing annotations
//---------------------------------------------------------------------------

annotations:
  annotations COLONCOLON annotation {
    $$.clear();
    $$.swap($1);
    $$.push_back($3);
  }
| /* empty */ { $$.clear(); }

annotation_arguments:  // Cannot be empty.
  annotation_arguments ',' annotation  {
    $$.clear();
    $$.swap($1);
    $$.push_back($3);
  }
| annotation { $$.clear(); $$.push_back($1); }

annotation:
  IVALUE DOTDOT IVALUE { $$ = FzAnnotation::Interval($1, $3); }
| IDENTIFIER {
  const std::string& id = $1;
  if (ContainsKey(context->variable_map, id)) {
    $$ = FzAnnotation::Variable(FindOrDie(context->variable_map, id));
  } else if (ContainsKey(context->variable_array_map, id)) {
    $$ = FzAnnotation::VariableList(FindOrDie(context->variable_array_map, id));
  } else {
    $$ = FzAnnotation::Identifier(id);
  }
}
| IDENTIFIER '(' annotation_arguments ')' {
  $$ = FzAnnotation::FunctionCall($1, $3);
}
| IDENTIFIER '[' IVALUE ']' {
  CHECK(ContainsKey(context->variable_array_map, $1))
      << "Unknown identifier: " << $1;
  $$ = FzAnnotation::Variable(
      FzLookup(FindOrDie(context->variable_array_map, $1), $3));
}
| '[' annotation_arguments ']' { $$ = FzAnnotation::AnnotationList($2); }

//---------------------------------------------------------------------------
// Parsing solve declaration.
//---------------------------------------------------------------------------

solve:
  SOLVE annotations SATISFY {
  model->Satisfy($2);
}
| SOLVE annotations MINIMIZE argument {
  CHECK_EQ(FzArgument::INT_VAR_REF, $4.type);
  model->Minimize($4.variable, $2);
}
| SOLVE annotations MAXIMIZE argument {
  CHECK_EQ(FzArgument::INT_VAR_REF, $4.type);
  model->Maximize($4.variable, $2);
}

%%
