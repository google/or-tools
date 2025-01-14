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

#ifndef OR_TOOLS_SAT_PYTHON_LINEAR_EXPR_DOC_H_
#define OR_TOOLS_SAT_PYTHON_LINEAR_EXPR_DOC_H_

// NOLINTBEGIN

/*
  This file contains docstrings for use in the Python bindings.
  Do not edit! They were automatically extracted by pybind11_mkdoc.
 */

#define __EXPAND(x) x
#define __COUNT(_1, _2, _3, _4, _5, _6, _7, COUNT, ...) COUNT
#define __VA_SIZE(...) __EXPAND(__COUNT(__VA_ARGS__, 7, 6, 5, 4, 3, 2, 1))
#define __CAT1(a, b) a##b
#define __CAT2(a, b) __CAT1(a, b)
#define __DOC1(n1) __doc_##n1
#define __DOC2(n1, n2) __doc_##n1##_##n2
#define __DOC3(n1, n2, n3) __doc_##n1##_##n2##_##n3
#define __DOC4(n1, n2, n3, n4) __doc_##n1##_##n2##_##n3##_##n4
#define __DOC5(n1, n2, n3, n4, n5) __doc_##n1##_##n2##_##n3##_##n4##_##n5
#define __DOC6(n1, n2, n3, n4, n5, n6) \
  __doc_##n1##_##n2##_##n3##_##n4##_##n5##_##n6
#define __DOC7(n1, n2, n3, n4, n5, n6, n7) \
  __doc_##n1##_##n2##_##n3##_##n4##_##n5##_##n6##_##n7
#define DOC(...) \
  __EXPAND(__EXPAND(__CAT2(__DOC, __VA_SIZE(__VA_ARGS__)))(__VA_ARGS__))

#if defined(__GNUG__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-variable"
#endif

static const char* __doc_operations_research_sat_python_AbslHashValue =
    R"doc()doc";

static const char* __doc_operations_research_sat_python_BaseIntVar =
    R"doc(A class to hold a variable index. It is the base class for Integer
variables.)doc";

static const char* __doc_operations_research_sat_python_BaseIntVar_2 =
    R"doc(A class to hold a variable index. It is the base class for Integer
variables.)doc";

static const char* __doc_operations_research_sat_python_BaseIntVarComparator =
    R"doc(Compare the indices of variables.)doc";

static const char*
    __doc_operations_research_sat_python_BaseIntVarComparator_operator_call =
        R"doc()doc";

static const char* __doc_operations_research_sat_python_BaseIntVar_BaseIntVar =
    R"doc()doc";

static const char*
    __doc_operations_research_sat_python_BaseIntVar_BaseIntVar_2 = R"doc()doc";

static const char* __doc_operations_research_sat_python_BaseIntVar_DebugString =
    R"doc()doc";

static const char* __doc_operations_research_sat_python_BaseIntVar_ToString =
    R"doc()doc";

static const char*
    __doc_operations_research_sat_python_BaseIntVar_VisitAsFloat = R"doc()doc";

static const char* __doc_operations_research_sat_python_BaseIntVar_VisitAsInt =
    R"doc()doc";

static const char* __doc_operations_research_sat_python_BaseIntVar_index =
    R"doc()doc";

static const char* __doc_operations_research_sat_python_BaseIntVar_index_2 =
    R"doc()doc";

static const char* __doc_operations_research_sat_python_BaseIntVar_is_boolean =
    R"doc(Returns true if the variable has a Boolean domain (0 or 1).)doc";

static const char* __doc_operations_research_sat_python_BaseIntVar_negated =
    R"doc(Returns the negation of the current variable.)doc";

static const char* __doc_operations_research_sat_python_BaseIntVar_negated_2 =
    R"doc()doc";

static const char* __doc_operations_research_sat_python_BaseIntVar_operator_lt =
    R"doc()doc";

static const char*
    __doc_operations_research_sat_python_BoundedLinearExpression =
        R"doc(A class to hold a linear expression with bounds.)doc";

static const char*
    __doc_operations_research_sat_python_BoundedLinearExpression_2 =
        R"doc(A class to hold a linear expression with bounds.)doc";

static const char*
    __doc_operations_research_sat_python_BoundedLinearExpression_BoundedLinearExpression =
        R"doc(Creates a BoundedLinearExpression representing `expr in domain`.)doc";

static const char*
    __doc_operations_research_sat_python_BoundedLinearExpression_BoundedLinearExpression_2 =
        R"doc(Creates a BoundedLinearExpression representing `pos - neg in domain`.)doc";

static const char*
    __doc_operations_research_sat_python_BoundedLinearExpression_CastToBool =
        R"doc()doc";

static const char*
    __doc_operations_research_sat_python_BoundedLinearExpression_DebugString =
        R"doc()doc";

static const char*
    __doc_operations_research_sat_python_BoundedLinearExpression_ToString =
        R"doc()doc";

static const char*
    __doc_operations_research_sat_python_BoundedLinearExpression_bounds =
        R"doc()doc";

static const char*
    __doc_operations_research_sat_python_BoundedLinearExpression_bounds_2 =
        R"doc()doc";

static const char*
    __doc_operations_research_sat_python_BoundedLinearExpression_coeffs =
        R"doc()doc";

static const char*
    __doc_operations_research_sat_python_BoundedLinearExpression_coeffs_2 =
        R"doc()doc";

static const char*
    __doc_operations_research_sat_python_BoundedLinearExpression_offset =
        R"doc()doc";

static const char*
    __doc_operations_research_sat_python_BoundedLinearExpression_offset_2 =
        R"doc()doc";

static const char*
    __doc_operations_research_sat_python_BoundedLinearExpression_ok =
        R"doc()doc";

static const char*
    __doc_operations_research_sat_python_BoundedLinearExpression_ok_2 =
        R"doc()doc";

static const char*
    __doc_operations_research_sat_python_BoundedLinearExpression_vars =
        R"doc()doc";

static const char*
    __doc_operations_research_sat_python_BoundedLinearExpression_vars_2 =
        R"doc()doc";

static const char* __doc_operations_research_sat_python_ExprOrValue =
    R"doc(A class to hold a pointer to a linear expression or a constant.)doc";

static const char*
    __doc_operations_research_sat_python_ExprOrValue_ExprOrValue = R"doc()doc";

static const char*
    __doc_operations_research_sat_python_ExprOrValue_ExprOrValue_2 =
        R"doc()doc";

static const char*
    __doc_operations_research_sat_python_ExprOrValue_ExprOrValue_3 =
        R"doc()doc";

static const char*
    __doc_operations_research_sat_python_ExprOrValue_double_value = R"doc()doc";

static const char* __doc_operations_research_sat_python_ExprOrValue_expr =
    R"doc()doc";

static const char* __doc_operations_research_sat_python_ExprOrValue_int_value =
    R"doc()doc";

static const char* __doc_operations_research_sat_python_FlatFloatExpr =
    R"doc(A flattened and optimized floating point linear expression.

It can be used to cache complex expressions as parsing them is only
done once.)doc";

static const char* __doc_operations_research_sat_python_FlatFloatExpr_2 =
    R"doc(A flattened and optimized floating point linear expression.

It can be used to cache complex expressions as parsing them is only
done once.)doc";

static const char*
    __doc_operations_research_sat_python_FlatFloatExpr_DebugString =
        R"doc()doc";

static const char*
    __doc_operations_research_sat_python_FlatFloatExpr_FlatFloatExpr =
        R"doc()doc";

static const char* __doc_operations_research_sat_python_FlatFloatExpr_ToString =
    R"doc()doc";

static const char*
    __doc_operations_research_sat_python_FlatFloatExpr_VisitAsFloat =
        R"doc()doc";

static const char*
    __doc_operations_research_sat_python_FlatFloatExpr_VisitAsInt = R"doc()doc";

static const char* __doc_operations_research_sat_python_FlatFloatExpr_coeffs =
    R"doc()doc";

static const char* __doc_operations_research_sat_python_FlatFloatExpr_coeffs_2 =
    R"doc()doc";

static const char* __doc_operations_research_sat_python_FlatFloatExpr_offset =
    R"doc()doc";

static const char* __doc_operations_research_sat_python_FlatFloatExpr_offset_2 =
    R"doc()doc";

static const char* __doc_operations_research_sat_python_FlatFloatExpr_vars =
    R"doc()doc";

static const char* __doc_operations_research_sat_python_FlatFloatExpr_vars_2 =
    R"doc()doc";

static const char* __doc_operations_research_sat_python_FlatIntExpr =
    R"doc(A flattened and optimized integer linear expression.

It can be used to cache complex expressions as parsing them is only
done once.)doc";

static const char*
    __doc_operations_research_sat_python_FlatIntExpr_DebugString = R"doc()doc";

static const char*
    __doc_operations_research_sat_python_FlatIntExpr_FlatIntExpr = R"doc()doc";

static const char* __doc_operations_research_sat_python_FlatIntExpr_ToString =
    R"doc()doc";

static const char*
    __doc_operations_research_sat_python_FlatIntExpr_VisitAsFloat = R"doc()doc";

static const char* __doc_operations_research_sat_python_FlatIntExpr_VisitAsInt =
    R"doc()doc";

static const char* __doc_operations_research_sat_python_FlatIntExpr_coeffs =
    R"doc()doc";

static const char* __doc_operations_research_sat_python_FlatIntExpr_coeffs_2 =
    R"doc()doc";

static const char* __doc_operations_research_sat_python_FlatIntExpr_offset =
    R"doc()doc";

static const char* __doc_operations_research_sat_python_FlatIntExpr_offset_2 =
    R"doc()doc";

static const char* __doc_operations_research_sat_python_FlatIntExpr_ok =
    R"doc()doc";

static const char* __doc_operations_research_sat_python_FlatIntExpr_ok_2 =
    R"doc()doc";

static const char* __doc_operations_research_sat_python_FlatIntExpr_vars =
    R"doc()doc";

static const char* __doc_operations_research_sat_python_FlatIntExpr_vars_2 =
    R"doc()doc";

static const char* __doc_operations_research_sat_python_FloatAffine =
    R"doc(A class to hold linear_expr * a = b (a and b are floating point
numbers).)doc";

static const char*
    __doc_operations_research_sat_python_FloatAffine_DebugString = R"doc()doc";

static const char*
    __doc_operations_research_sat_python_FloatAffine_FloatAffine = R"doc()doc";

static const char* __doc_operations_research_sat_python_FloatAffine_ToString =
    R"doc()doc";

static const char*
    __doc_operations_research_sat_python_FloatAffine_VisitAsFloat = R"doc()doc";

static const char* __doc_operations_research_sat_python_FloatAffine_VisitAsInt =
    R"doc()doc";

static const char* __doc_operations_research_sat_python_FloatAffine_coeff =
    R"doc()doc";

static const char*
    __doc_operations_research_sat_python_FloatAffine_coefficient = R"doc()doc";

static const char* __doc_operations_research_sat_python_FloatAffine_expr =
    R"doc()doc";

static const char* __doc_operations_research_sat_python_FloatAffine_expression =
    R"doc()doc";

static const char* __doc_operations_research_sat_python_FloatAffine_offset =
    R"doc()doc";

static const char* __doc_operations_research_sat_python_FloatAffine_offset_2 =
    R"doc()doc";

static const char* __doc_operations_research_sat_python_FloatConstant =
    R"doc(A class to hold a floating point constant as a linear expression.)doc";

static const char*
    __doc_operations_research_sat_python_FloatConstant_DebugString =
        R"doc()doc";

static const char*
    __doc_operations_research_sat_python_FloatConstant_FloatConstant =
        R"doc()doc";

static const char* __doc_operations_research_sat_python_FloatConstant_ToString =
    R"doc()doc";

static const char*
    __doc_operations_research_sat_python_FloatConstant_VisitAsFloat =
        R"doc()doc";

static const char*
    __doc_operations_research_sat_python_FloatConstant_VisitAsInt = R"doc()doc";

static const char* __doc_operations_research_sat_python_FloatConstant_value =
    R"doc()doc";

static const char* __doc_operations_research_sat_python_FloatExprVisitor =
    R"doc(A visitor class to process a floating point linear expression.)doc";

static const char* __doc_operations_research_sat_python_FloatExprVisitor_2 =
    R"doc(A visitor class to process a floating point linear expression.)doc";

static const char*
    __doc_operations_research_sat_python_FloatExprVisitor_AddConstant =
        R"doc()doc";

static const char*
    __doc_operations_research_sat_python_FloatExprVisitor_AddToProcess =
        R"doc()doc";

static const char*
    __doc_operations_research_sat_python_FloatExprVisitor_AddVarCoeff =
        R"doc()doc";

static const char*
    __doc_operations_research_sat_python_FloatExprVisitor_Process = R"doc()doc";

static const char*
    __doc_operations_research_sat_python_FloatExprVisitor_canonical_terms =
        R"doc()doc";

static const char*
    __doc_operations_research_sat_python_FloatExprVisitor_offset = R"doc()doc";

static const char*
    __doc_operations_research_sat_python_FloatExprVisitor_to_process =
        R"doc()doc";

static const char* __doc_operations_research_sat_python_FloatWeightedSum =
    R"doc(A class to hold a weighted sum of floating point linear expressions.
*/)doc";

static const char*
    __doc_operations_research_sat_python_FloatWeightedSum_DebugString =
        R"doc()doc";

static const char*
    __doc_operations_research_sat_python_FloatWeightedSum_FloatWeightedSum =
        R"doc()doc";

static const char*
    __doc_operations_research_sat_python_FloatWeightedSum_FloatWeightedSum_2 =
        R"doc()doc";

static const char*
    __doc_operations_research_sat_python_FloatWeightedSum_ToString =
        R"doc()doc";

static const char*
    __doc_operations_research_sat_python_FloatWeightedSum_VisitAsFloat =
        R"doc()doc";

static const char*
    __doc_operations_research_sat_python_FloatWeightedSum_VisitAsInt =
        R"doc()doc";

static const char*
    __doc_operations_research_sat_python_FloatWeightedSum_coeffs = R"doc()doc";

static const char* __doc_operations_research_sat_python_FloatWeightedSum_exprs =
    R"doc()doc";

static const char*
    __doc_operations_research_sat_python_FloatWeightedSum_offset = R"doc()doc";

static const char* __doc_operations_research_sat_python_IntAffine =
    R"doc(A class to hold linear_expr * a = b (a and b are integers).)doc";

static const char* __doc_operations_research_sat_python_IntAffine_DebugString =
    R"doc()doc";

static const char* __doc_operations_research_sat_python_IntAffine_IntAffine =
    R"doc()doc";

static const char* __doc_operations_research_sat_python_IntAffine_ToString =
    R"doc()doc";

static const char* __doc_operations_research_sat_python_IntAffine_VisitAsFloat =
    R"doc()doc";

static const char* __doc_operations_research_sat_python_IntAffine_VisitAsInt =
    R"doc()doc";

static const char* __doc_operations_research_sat_python_IntAffine_coeff =
    R"doc()doc";

static const char* __doc_operations_research_sat_python_IntAffine_coefficient =
    R"doc()doc";

static const char* __doc_operations_research_sat_python_IntAffine_expr =
    R"doc()doc";

static const char* __doc_operations_research_sat_python_IntAffine_expression =
    R"doc()doc";

static const char* __doc_operations_research_sat_python_IntAffine_offset =
    R"doc()doc";

static const char* __doc_operations_research_sat_python_IntAffine_offset_2 =
    R"doc()doc";

static const char* __doc_operations_research_sat_python_IntConstant =
    R"doc(A class to hold an integer constant as a linear expression.)doc";

static const char*
    __doc_operations_research_sat_python_IntConstant_DebugString = R"doc()doc";

static const char*
    __doc_operations_research_sat_python_IntConstant_IntConstant = R"doc()doc";

static const char* __doc_operations_research_sat_python_IntConstant_ToString =
    R"doc()doc";

static const char*
    __doc_operations_research_sat_python_IntConstant_VisitAsFloat = R"doc()doc";

static const char* __doc_operations_research_sat_python_IntConstant_VisitAsInt =
    R"doc()doc";

static const char* __doc_operations_research_sat_python_IntConstant_value =
    R"doc()doc";

static const char* __doc_operations_research_sat_python_IntExprVisitor =
    R"doc(A visitor class to process an integer linear expression.)doc";

static const char* __doc_operations_research_sat_python_IntExprVisitor_2 =
    R"doc(A visitor class to process an integer linear expression.)doc";

static const char*
    __doc_operations_research_sat_python_IntExprVisitor_AddConstant =
        R"doc()doc";

static const char*
    __doc_operations_research_sat_python_IntExprVisitor_AddToProcess =
        R"doc()doc";

static const char*
    __doc_operations_research_sat_python_IntExprVisitor_AddVarCoeff =
        R"doc()doc";

static const char*
    __doc_operations_research_sat_python_IntExprVisitor_Evaluate = R"doc()doc";

static const char* __doc_operations_research_sat_python_IntExprVisitor_Process =
    R"doc()doc";

static const char*
    __doc_operations_research_sat_python_IntExprVisitor_ProcessAll =
        R"doc()doc";

static const char*
    __doc_operations_research_sat_python_IntExprVisitor_canonical_terms =
        R"doc()doc";

static const char* __doc_operations_research_sat_python_IntExprVisitor_offset =
    R"doc()doc";

static const char*
    __doc_operations_research_sat_python_IntExprVisitor_to_process =
        R"doc()doc";

static const char* __doc_operations_research_sat_python_IntWeightedSum =
    R"doc(A class to hold a weighted sum of integer linear expressions.)doc";

static const char*
    __doc_operations_research_sat_python_IntWeightedSum_DebugString =
        R"doc()doc";

static const char*
    __doc_operations_research_sat_python_IntWeightedSum_IntWeightedSum =
        R"doc()doc";

static const char*
    __doc_operations_research_sat_python_IntWeightedSum_ToString = R"doc()doc";

static const char*
    __doc_operations_research_sat_python_IntWeightedSum_VisitAsFloat =
        R"doc()doc";

static const char*
    __doc_operations_research_sat_python_IntWeightedSum_VisitAsInt =
        R"doc()doc";

static const char* __doc_operations_research_sat_python_IntWeightedSum_coeffs =
    R"doc()doc";

static const char* __doc_operations_research_sat_python_IntWeightedSum_exprs =
    R"doc()doc";

static const char* __doc_operations_research_sat_python_IntWeightedSum_offset =
    R"doc()doc";

static const char* __doc_operations_research_sat_python_LinearExpr =
    R"doc(A class to hold an integer or floating point linear expression.

A linear expression is built from (integer or floating point)
constants and variables. For example, `x + 2 * (y - z + 1)`.

Linear expressions are used in CP-SAT models in constraints and in the
objective.

Note that constraints only accept linear expressions with integral
coefficients and constants. On the other hand, The objective can be a
linear expression with floating point coefficients and constants.

You can define linear constraints as in:

```
model.add(x + 2 * y <= 5)
model.add(sum(array_of_vars) == 5)
```

- In CP-SAT, the objective is a linear expression:

```
model.minimize(x + 2 * y + z)
```

- For large arrays, using the LinearExpr class is faster that using
the python `sum()` function. You can create constraints and the
objective from lists of linear expressions or coefficients as follows:

```
model.minimize(cp_model.LinearExpr.sum(expressions))
model.add(cp_model.LinearExpr.weighted_sum(expressions, coefficients) >= 0)
```)doc";

static const char* __doc_operations_research_sat_python_LinearExpr_2 =
    R"doc(A class to hold an integer or floating point linear expression.

A linear expression is built from (integer or floating point)
constants and variables. For example, `x + 2 * (y - z + 1)`.

Linear expressions are used in CP-SAT models in constraints and in the
objective.

Note that constraints only accept linear expressions with integral
coefficients and constants. On the other hand, The objective can be a
linear expression with floating point coefficients and constants.

You can define linear constraints as in:

```
model.add(x + 2 * y <= 5)
model.add(sum(array_of_vars) == 5)
```

- In CP-SAT, the objective is a linear expression:

```
model.minimize(x + 2 * y + z)
```

- For large arrays, using the LinearExpr class is faster that using
the python `sum()` function. You can create constraints and the
objective from lists of linear expressions or coefficients as follows:

```
model.minimize(cp_model.LinearExpr.sum(expressions))
model.add(cp_model.LinearExpr.weighted_sum(expressions, coefficients) >= 0)
```)doc";

static const char* __doc_operations_research_sat_python_LinearExpr_3 =
    R"doc(A class to hold an integer or floating point linear expression.

A linear expression is built from (integer or floating point)
constants and variables. For example, `x + 2 * (y - z + 1)`.

Linear expressions are used in CP-SAT models in constraints and in the
objective.

Note that constraints only accept linear expressions with integral
coefficients and constants. On the other hand, The objective can be a
linear expression with floating point coefficients and constants.

You can define linear constraints as in:

```
model.add(x + 2 * y <= 5)
model.add(sum(array_of_vars) == 5)
```

- In CP-SAT, the objective is a linear expression:

```
model.minimize(x + 2 * y + z)
```

- For large arrays, using the LinearExpr class is faster that using
the python `sum()` function. You can create constraints and the
objective from lists of linear expressions or coefficients as follows:

```
model.minimize(cp_model.LinearExpr.sum(expressions))
model.add(cp_model.LinearExpr.weighted_sum(expressions, coefficients) >= 0)
```)doc";

static const char* __doc_operations_research_sat_python_LinearExpr_Add =
    R"doc(Returns (this) + (expr).)doc";

static const char* __doc_operations_research_sat_python_LinearExpr_AddFloat =
    R"doc(Returns (this) + (cst).)doc";

static const char* __doc_operations_research_sat_python_LinearExpr_AddInt =
    R"doc(Returns (this) + (cst).)doc";

static const char* __doc_operations_research_sat_python_LinearExpr_AffineFloat =
    R"doc(Returns expr * coeff + offset.)doc";

static const char* __doc_operations_research_sat_python_LinearExpr_AffineInt =
    R"doc(Returns expr * coeff + offset.)doc";

static const char*
    __doc_operations_research_sat_python_LinearExpr_ConstantFloat =
        R"doc(Returns a new LinearExpr that is the given constant.)doc";

static const char* __doc_operations_research_sat_python_LinearExpr_ConstantInt =
    R"doc(Returns a new LinearExpr that is the given constant.)doc";

static const char* __doc_operations_research_sat_python_LinearExpr_DebugString =
    R"doc()doc";

static const char* __doc_operations_research_sat_python_LinearExpr_Eq =
    R"doc(Returns (this) == (rhs).)doc";

static const char* __doc_operations_research_sat_python_LinearExpr_EqCst =
    R"doc(Returns (this) == (rhs).)doc";

static const char* __doc_operations_research_sat_python_LinearExpr_Ge =
    R"doc(Returns (this) >= (rhs).)doc";

static const char* __doc_operations_research_sat_python_LinearExpr_GeCst =
    R"doc(Returns (this) >= (rhs).)doc";

static const char* __doc_operations_research_sat_python_LinearExpr_Gt =
    R"doc(Returns (this) > (rhs).)doc";

static const char* __doc_operations_research_sat_python_LinearExpr_GtCst =
    R"doc(Returns (this) > (rhs).)doc";

static const char* __doc_operations_research_sat_python_LinearExpr_IsInteger =
    R"doc()doc";

static const char* __doc_operations_research_sat_python_LinearExpr_Le =
    R"doc(Returns (this) <= (rhs).)doc";

static const char* __doc_operations_research_sat_python_LinearExpr_LeCst =
    R"doc(Returns (this) <= (rhs).)doc";

static const char* __doc_operations_research_sat_python_LinearExpr_Lt =
    R"doc(Returns (this) < (rhs).)doc";

static const char* __doc_operations_research_sat_python_LinearExpr_LtCst =
    R"doc(Returns (this) < (rhs).)doc";

static const char* __doc_operations_research_sat_python_LinearExpr_MixedSum =
    R"doc(Returns a new LinearExpr that is the sum of the given expressions or
constants.)doc";

static const char*
    __doc_operations_research_sat_python_LinearExpr_MixedWeightedSumFloat =
        R"doc(Returns the sum(exprs[i] * coeffs[i]).)doc";

static const char*
    __doc_operations_research_sat_python_LinearExpr_MixedWeightedSumInt =
        R"doc(Returns the sum(exprs[i] * coeffs[i]).)doc";

static const char* __doc_operations_research_sat_python_LinearExpr_MulFloat =
    R"doc(Returns (this) * (cst).)doc";

static const char* __doc_operations_research_sat_python_LinearExpr_MulInt =
    R"doc(Returns (this) * (cst).)doc";

static const char* __doc_operations_research_sat_python_LinearExpr_Ne =
    R"doc(Returns (this) != (rhs).)doc";

static const char* __doc_operations_research_sat_python_LinearExpr_NeCst =
    R"doc(Returns (this) != (rhs).)doc";

static const char* __doc_operations_research_sat_python_LinearExpr_Neg =
    R"doc(Returns -(this).)doc";

static const char* __doc_operations_research_sat_python_LinearExpr_RSubFloat =
    R"doc(Returns (cst) - (this).)doc";

static const char* __doc_operations_research_sat_python_LinearExpr_RSubInt =
    R"doc(Returns (cst) - (this).)doc";

static const char* __doc_operations_research_sat_python_LinearExpr_Sub =
    R"doc(Returns (this) - (expr).)doc";

static const char* __doc_operations_research_sat_python_LinearExpr_SubFloat =
    R"doc(Returns (this) - (cst).)doc";

static const char* __doc_operations_research_sat_python_LinearExpr_SubInt =
    R"doc(Returns (this) - (cst).)doc";

static const char* __doc_operations_research_sat_python_LinearExpr_Sum =
    R"doc(Returns a new LinearExpr that is the sum of the given expressions.)doc";

static const char* __doc_operations_research_sat_python_LinearExpr_TermFloat =
    R"doc(Returns expr * coeff.)doc";

static const char* __doc_operations_research_sat_python_LinearExpr_TermInt =
    R"doc(Returns expr * coeff.)doc";

static const char* __doc_operations_research_sat_python_LinearExpr_ToString =
    R"doc()doc";

static const char*
    __doc_operations_research_sat_python_LinearExpr_VisitAsFloat = R"doc()doc";

static const char* __doc_operations_research_sat_python_LinearExpr_VisitAsInt =
    R"doc()doc";

static const char*
    __doc_operations_research_sat_python_LinearExpr_WeightedSumFloat =
        R"doc(Returns the sum(exprs[i] * coeffs[i]).)doc";

static const char*
    __doc_operations_research_sat_python_LinearExpr_WeightedSumInt =
        R"doc(Returns the sum(exprs[i] * coeffs[i]).)doc";

static const char* __doc_operations_research_sat_python_Literal =
    R"doc(A class to hold a Boolean literal.

A literal is a Boolean variable or its negation.

Literals are used in CP-SAT models in constraints and in the
objective.

- You can define literal as in:

```
b1 = model.new_bool_var()
b2 = model.new_bool_var()
# Simple Boolean constraint.
model.add_bool_or(b1, b2.negated())
# We can use the ~ operator to negate a literal.
model.add_bool_or(b1, ~b2)
# Enforcement literals must be literals.
x = model.new_int_var(0, 10, 'x')
model.add(x == 5).only_enforced_if(~b1)
```

- Literals can be used directly in linear constraints or in the
objective:

```
model.minimize(b1  + 2 * ~b2)
```)doc";

static const char* __doc_operations_research_sat_python_Literal_index =
    R"doc(Returns the index of the current literal.)doc";

static const char* __doc_operations_research_sat_python_Literal_negated =
    R"doc(Returns the negation of a literal (a Boolean variable or its
negation).

This method implements the logical negation of a Boolean variable. It
is only valid if the variable has a Boolean domain (0 or 1).

Note that this method is nilpotent: `x.negated().negated() == x`.

Returns: The negation of the current literal.)doc";

static const char* __doc_operations_research_sat_python_NotBooleanVariable =
    R"doc(A class to hold a negated variable index.)doc";

static const char* __doc_operations_research_sat_python_NotBooleanVariable_2 =
    R"doc(A class to hold a negated variable index.)doc";

static const char*
    __doc_operations_research_sat_python_NotBooleanVariable_DebugString =
        R"doc()doc";

static const char*
    __doc_operations_research_sat_python_NotBooleanVariable_NotBooleanVariable =
        R"doc()doc";

static const char*
    __doc_operations_research_sat_python_NotBooleanVariable_ToString =
        R"doc()doc";

static const char*
    __doc_operations_research_sat_python_NotBooleanVariable_VisitAsFloat =
        R"doc()doc";

static const char*
    __doc_operations_research_sat_python_NotBooleanVariable_VisitAsInt =
        R"doc()doc";

static const char*
    __doc_operations_research_sat_python_NotBooleanVariable_index = R"doc()doc";

static const char*
    __doc_operations_research_sat_python_NotBooleanVariable_negated =
        R"doc(Returns the negation of the current literal, that is the original
Boolean variable.)doc";

static const char* __doc_operations_research_sat_python_NotBooleanVariable_var =
    R"doc()doc";

static const char* __doc_operations_research_sat_python_SumArray =
    R"doc(A class to hold a sum of linear expressions, and optional integer and
double offsets (at most one of them can be non-zero, this is
DCHECKed).)doc";

static const char* __doc_operations_research_sat_python_SumArray_DebugString =
    R"doc()doc";

static const char* __doc_operations_research_sat_python_SumArray_SumArray =
    R"doc()doc";

static const char* __doc_operations_research_sat_python_SumArray_ToString =
    R"doc()doc";

static const char* __doc_operations_research_sat_python_SumArray_VisitAsFloat =
    R"doc()doc";

static const char* __doc_operations_research_sat_python_SumArray_VisitAsInt =
    R"doc()doc";

static const char* __doc_operations_research_sat_python_SumArray_double_offset =
    R"doc()doc";

static const char* __doc_operations_research_sat_python_SumArray_exprs =
    R"doc()doc";

static const char* __doc_operations_research_sat_python_SumArray_int_offset =
    R"doc()doc";

#if defined(__GNUG__)
#pragma GCC diagnostic pop
#endif

// NOLINTEND

#endif  // OR_TOOLS_SAT_PYTHON_LINEAR_EXPR_DOC_H_
