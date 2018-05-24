# Boolean logic recipes for the CP-SAT solver.



## Introduction

The CP-SAT solver can express boolean variables and constraints. A **boolean
variable** is a integer variable between 0 and 1. A **literal** is either a
boolean variable or its negation. Please note that this negation is different
from the opposite operation on integer variables.

## Boolean variables and literals

### Python code

    from google3.util.operations_research.sat.python import cp_model

    model = cp_model.CpModel()
    x = model.NewBoolVar('x')
    literal = x.Not()

### C++ code

    #include "ortools/sat/cp_model.pb.h"
    #include "ortools/sat/cp_model_utils.h"

    namespace operations_research {
    namespace sat {

    CpModelProto cp_model;

    auto new_boolean_variable = [&cp_model]() {
      const int index = cp_model.variables_size();
      IntegerVariableProto* const var = cp_model.add_variables();
      var->add_domain(0);
      var->add_domain(1);
      return index;
    };

    const int x = new_boolean_variable();
    const int literal = NegatedRef(x);

    }  // namespace sat
    }  // namespace operations_research

## Boolean constraints

There are three boolean constraints.

-   or(literal1, .., literaln)
-   and(literal1, .., literaln)
-   xor(literal1, .., literaln)

### Python code

    from google3.util.operations_research.sat.python import cp_model

    model = cp_model.CpModel()
    x = model.NewBoolVar('x')
    y = model.NewBoolVar('y')
    model.AddBoolOr([x, y.Not()])

### C++ code

    #include "ortools/sat/cp_model.pb.h"
    #include "ortools/sat/cp_model_utils.h"

    namespace operations_research {
    namespace sat {

    CpModelProto cp_model;

    auto new_boolean_variable = [&cp_model]() {
      const int index = cp_model.variables_size();
      IntegerVariableProto* const var = cp_model.add_variables();
      var->add_domain(0);
      var->add_domain(1);
      return index;
    };

    auto add_bool_or = [&cp_model](const std::vector<int>& literals) {
      BooleanArgumentProto* const bool_or =
          model.add_constraints()->mutable_bool_or();
      for (const int lit : literals) {
        mutable_bool_or->add_literals(lit);
      }
    };

    const int x = new_boolean_variable();
    const int y = new_boolean_variable();
    add_bool_or({x, NegatedRef(y)});

    }  // namespace sat
    }  // namespace operations_research
