// Copyright 2010-2013 Google
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
#include <string>
#include "base/integral_types.h"
#include "base/logging.h"
#include "base/hash.h"
#include "flatzinc2/model.h"
#include "flatzinc2/search.h"
#include "flatzinc2/solver.h"
#include "constraint_solver/constraint_solver.h"
#include "util/string_array.h"

namespace operations_research {
void ExtractAllDifferentInt(FzSolver* const solver, FzConstraint* const ct) {
  Solver* const s = solver->solver();
  const std::vector<IntVar*> vars = solver->GetVariableArray(ct->arguments[0]);
  s->AddConstraint(s->MakeAllDifferent(vars));
}

void ExtractAlldifferentExcept0(FzSolver* const solver,
                                FzConstraint* const ct) {
  Solver* const s = solver->solver();
  const std::vector<IntVar*> vars = solver->GetVariableArray(ct->arguments[0]);
  s->AddConstraint(s->MakeAllDifferentExcept(vars, 0));
}

void ExtractArrayBoolAnd(FzSolver* const solver, FzConstraint* const ct) {
  LOG(FATAL) << "Not implemented: Extract " << ct->DebugString();
}

void ExtractArrayBoolOr(FzSolver* const solver, FzConstraint* const ct) {
  LOG(FATAL) << "Not implemented: Extract " << ct->DebugString();
}

void ExtractArrayBoolXor(FzSolver* const solver, FzConstraint* const ct) {
  LOG(FATAL) << "Not implemented: Extract " << ct->DebugString();
}

void ExtractArrayIntElement(FzSolver* const solver, FzConstraint* const ct) {
  LOG(FATAL) << "Not implemented: Extract " << ct->DebugString();
}

void ExtractArrayVarIntElement(FzSolver* const solver, FzConstraint* const ct) {
  LOG(FATAL) << "Not implemented: Extract " << ct->DebugString();
}

void ExtractArrayVarIntPosition(FzSolver* const solver,
                                FzConstraint* const ct) {
  LOG(FATAL) << "Not implemented: Extract " << ct->DebugString();
}

void ExtractBool2int(FzSolver* const solver, FzConstraint* const ct) {
  LOG(FATAL) << "Constraint should have been presolved out: "
             << ct->DebugString();
}

void ExtractBoolAnd(FzSolver* const solver, FzConstraint* const ct) {
  LOG(FATAL) << "Not implemented: Extract " << ct->DebugString();
}

void ExtractBoolClause(FzSolver* const solver, FzConstraint* const ct) {
  LOG(FATAL) << "Not implemented: Extract " << ct->DebugString();
}

void ExtractBoolLeftImp(FzSolver* const solver, FzConstraint* const ct) {
  LOG(FATAL) << "Not implemented: Extract " << ct->DebugString();
}

void ExtractBoolNot(FzSolver* const solver, FzConstraint* const ct) {
  LOG(FATAL) << "Not implemented: Extract " << ct->DebugString();
}

void ExtractBoolOr(FzSolver* const solver, FzConstraint* const ct) {
  LOG(FATAL) << "Not implemented: Extract " << ct->DebugString();
}

void ExtractBoolRightImp(FzSolver* const solver, FzConstraint* const ct) {
  LOG(FATAL) << "Not implemented: Extract " << ct->DebugString();
}

void ExtractBoolXor(FzSolver* const solver, FzConstraint* const ct) {
  LOG(FATAL) << "Not implemented: Extract " << ct->DebugString();
}

void ExtractCircuit(FzSolver* const solver, FzConstraint* const ct) {
  LOG(FATAL) << "Not implemented: Extract " << ct->DebugString();
}

void ExtractCountEq(FzSolver* const solver, FzConstraint* const ct) {
  LOG(FATAL) << "Not implemented: Extract " << ct->DebugString();
}

void ExtractCountGeq(FzSolver* const solver, FzConstraint* const ct) {
  LOG(FATAL) << "Not implemented: Extract " << ct->DebugString();
}

void ExtractCountGt(FzSolver* const solver, FzConstraint* const ct) {
  LOG(FATAL) << "Not implemented: Extract " << ct->DebugString();
}

void ExtractCountLeq(FzSolver* const solver, FzConstraint* const ct) {
  LOG(FATAL) << "Not implemented: Extract " << ct->DebugString();
}

void ExtractCountLt(FzSolver* const solver, FzConstraint* const ct) {
  LOG(FATAL) << "Not implemented: Extract " << ct->DebugString();
}

void ExtractCountNeq(FzSolver* const solver, FzConstraint* const ct) {
  LOG(FATAL) << "Not implemented: Extract " << ct->DebugString();
}

void ExtractCountReif(FzSolver* const solver, FzConstraint* const ct) {
  LOG(FATAL) << "Not implemented: Extract " << ct->DebugString();
}

void ExtractDiffn(FzSolver* const solver, FzConstraint* const ct) {
  LOG(FATAL) << "Not implemented: Extract " << ct->DebugString();
}

void ExtractFixedCumulative(FzSolver* const solver, FzConstraint* const ct) {
  LOG(FATAL) << "Not implemented: Extract " << ct->DebugString();
}

void ExtractGlobalCardinality(FzSolver* const solver, FzConstraint* const ct) {
  LOG(FATAL) << "Not implemented: Extract " << ct->DebugString();
}

void ExtractGlobalCardinalityClosed(FzSolver* const solver,
                                    FzConstraint* const ct) {
  LOG(FATAL) << "Not implemented: Extract " << ct->DebugString();
}

void ExtractGlobalCardinalityLowUp(FzSolver* const solver,
                                   FzConstraint* const ct) {
  LOG(FATAL) << "Not implemented: Extract " << ct->DebugString();
}

void ExtractGlobalCardinalityLowUpClosed(FzSolver* const solver,
                                         FzConstraint* const ct) {
  LOG(FATAL) << "Not implemented: Extract " << ct->DebugString();
}

void ExtractGlobalCardinalityOld(FzSolver* const solver,
                                 FzConstraint* const ct) {
  LOG(FATAL) << "Not implemented: Extract " << ct->DebugString();
}

void ExtractIntAbs(FzSolver* const solver, FzConstraint* const ct) {
  LOG(FATAL) << "Not implemented: Extract " << ct->DebugString();
}

void ExtractIntDiv(FzSolver* const solver, FzConstraint* const ct) {
  LOG(FATAL) << "Not implemented: Extract " << ct->DebugString();
}

void ExtractIntEq(FzSolver* const solver, FzConstraint* const ct) {
  Solver* const s = solver->solver();
  if (ct->arguments[0].type == FzArgument::INT_VAR_REF) {
    IntExpr* const left = solver->GetExpression(ct->arguments[0]);
    if (ct->arguments[1].type == FzArgument::INT_VAR_REF) {
      IntExpr* const right = solver->GetExpression(ct->arguments[1]);
      s->AddConstraint(s->MakeEquality(left, right));
    } else {
      const int64 right = ct->arguments[1].integer_value;
      s->AddConstraint(s->MakeEquality(left, right));
    }
  } else {
    const int64 left = ct->arguments[0].integer_value;
    if (ct->arguments[1].type == FzArgument::INT_VAR_REF) {
      IntExpr* const right = solver->GetExpression(ct->arguments[1]);
      s->AddConstraint(s->MakeEquality(right, left));
    } else {
      const int64 right = ct->arguments[1].integer_value;
      if (left != right) {
        s->AddConstraint(s->MakeFalseConstraint());
      }
    }
  }
}

void ExtractIntEqReif(FzSolver* const solver, FzConstraint* const ct) {
  LOG(FATAL) << "Not implemented: Extract " << ct->DebugString();
}

void ExtractIntGe(FzSolver* const solver, FzConstraint* const ct) {
  LOG(FATAL) << "Not implemented: Extract " << ct->DebugString();
}

void ExtractIntGeReif(FzSolver* const solver, FzConstraint* const ct) {
  LOG(FATAL) << "Not implemented: Extract " << ct->DebugString();
}

void ExtractIntGt(FzSolver* const solver, FzConstraint* const ct) {
  LOG(FATAL) << "Not implemented: Extract " << ct->DebugString();
}

void ExtractIntGtReif(FzSolver* const solver, FzConstraint* const ct) {
  LOG(FATAL) << "Not implemented: Extract " << ct->DebugString();
}

void ExtractIntIn(FzSolver* const solver, FzConstraint* const ct) {
  LOG(FATAL) << "Not implemented: Extract " << ct->DebugString();
}

void ExtractIntLe(FzSolver* const solver, FzConstraint* const ct) {
  LOG(FATAL) << "Not implemented: Extract " << ct->DebugString();
}

void ExtractIntLeReif(FzSolver* const solver, FzConstraint* const ct) {
  LOG(FATAL) << "Not implemented: Extract " << ct->DebugString();
}

void ExtractIntLinEq(FzSolver* const solver, FzConstraint* const ct) {
  Solver* const s = solver->solver();
  const std::vector<int64>& coefficients = ct->arguments[0].domain.values;
  std::vector<IntVar*> vars = solver->GetVariableArray(ct->arguments[1]);
  const int64 rhs = ct->arguments[2].integer_value;
  s->AddConstraint(s->MakeScalProdEquality(vars, coefficients, rhs));
}

void ExtractIntLinEqReif(FzSolver* const solver, FzConstraint* const ct) {
  LOG(FATAL) << "Not implemented: Extract " << ct->DebugString();
}

void ExtractIntLinGe(FzSolver* const solver, FzConstraint* const ct) {
  LOG(FATAL) << "Not implemented: Extract " << ct->DebugString();
}

void ExtractIntLinGeReif(FzSolver* const solver, FzConstraint* const ct) {
  LOG(FATAL) << "Not implemented: Extract " << ct->DebugString();
}

void ExtractIntLinLe(FzSolver* const solver, FzConstraint* const ct) {
  LOG(FATAL) << "Not implemented: Extract " << ct->DebugString();
}

void ExtractIntLinLeReif(FzSolver* const solver, FzConstraint* const ct) {
  LOG(FATAL) << "Not implemented: Extract " << ct->DebugString();
}

void ExtractIntLinNe(FzSolver* const solver, FzConstraint* const ct) {
  LOG(FATAL) << "Not implemented: Extract " << ct->DebugString();
}

void ExtractIntLinNeReif(FzSolver* const solver, FzConstraint* const ct) {
  LOG(FATAL) << "Not implemented: Extract " << ct->DebugString();
}

void ExtractIntLt(FzSolver* const solver, FzConstraint* const ct) {
  LOG(FATAL) << "Not implemented: Extract " << ct->DebugString();
}

void ExtractIntLtReif(FzSolver* const solver, FzConstraint* const ct) {
  LOG(FATAL) << "Not implemented: Extract " << ct->DebugString();
}

void ExtractIntMax(FzSolver* const solver, FzConstraint* const ct) {
  LOG(FATAL) << "Not implemented: Extract " << ct->DebugString();
}

void ExtractIntMin(FzSolver* const solver, FzConstraint* const ct) {
  LOG(FATAL) << "Not implemented: Extract " << ct->DebugString();
}

void ExtractIntMinus(FzSolver* const solver, FzConstraint* const ct) {
  LOG(FATAL) << "Not implemented: Extract " << ct->DebugString();
}

void ExtractIntMod(FzSolver* const solver, FzConstraint* const ct) {
  LOG(FATAL) << "Not implemented: Extract " << ct->DebugString();
}

void ExtractIntNe(FzSolver* const solver, FzConstraint* const ct) {
  LOG(FATAL) << "Not implemented: Extract " << ct->DebugString();
}

void ExtractIntNeReif(FzSolver* const solver, FzConstraint* const ct) {
  LOG(FATAL) << "Not implemented: Extract " << ct->DebugString();
}

void ExtractIntNegate(FzSolver* const solver, FzConstraint* const ct) {
  LOG(FATAL) << "Not implemented: Extract " << ct->DebugString();
}

void ExtractIntPlus(FzSolver* const solver, FzConstraint* const ct) {
  LOG(FATAL) << "Not implemented: Extract " << ct->DebugString();
}

void ExtractIntTimes(FzSolver* const solver, FzConstraint* const ct) {
  LOG(FATAL) << "Not implemented: Extract " << ct->DebugString();
}

void ExtractInverse(FzSolver* const solver, FzConstraint* const ct) {
  LOG(FATAL) << "Not implemented: Extract " << ct->DebugString();
}

void ExtractLexLessBool(FzSolver* const solver, FzConstraint* const ct) {
  LOG(FATAL) << "Not implemented: Extract " << ct->DebugString();
}

void ExtractLexLessInt(FzSolver* const solver, FzConstraint* const ct) {
  LOG(FATAL) << "Not implemented: Extract " << ct->DebugString();
}

void ExtractLexLesseqBool(FzSolver* const solver, FzConstraint* const ct) {
  LOG(FATAL) << "Not implemented: Extract " << ct->DebugString();
}

void ExtractLexLesseqInt(FzSolver* const solver, FzConstraint* const ct) {
  LOG(FATAL) << "Not implemented: Extract " << ct->DebugString();
}

void ExtractMaximumInt(FzSolver* const solver, FzConstraint* const ct) {
  LOG(FATAL) << "Not implemented: Extract " << ct->DebugString();
}

void ExtractMinimumInt(FzSolver* const solver, FzConstraint* const ct) {
  LOG(FATAL) << "Not implemented: Extract " << ct->DebugString();
}

void ExtractNvalue(FzSolver* const solver, FzConstraint* const ct) {
  LOG(FATAL) << "Not implemented: Extract " << ct->DebugString();
}

void ExtractRegular(FzSolver* const solver, FzConstraint* const ct) {
  LOG(FATAL) << "Not implemented: Extract " << ct->DebugString();
}

void ExtractSetIn(FzSolver* const solver, FzConstraint* const ct) {
  LOG(FATAL) << "Not implemented: Extract " << ct->DebugString();
}

void ExtractSetInReif(FzSolver* const solver, FzConstraint* const ct) {
  LOG(FATAL) << "Not implemented: Extract " << ct->DebugString();
}

void ExtractSlidingSum(FzSolver* const solver, FzConstraint* const ct) {
  LOG(FATAL) << "Not implemented: Extract " << ct->DebugString();
}

void ExtractSort(FzSolver* const solver, FzConstraint* const ct) {
  LOG(FATAL) << "Not implemented: Extract " << ct->DebugString();
}

void ExtractTableBool(FzSolver* const solver, FzConstraint* const ct) {
  LOG(FATAL) << "Not implemented: Extract " << ct->DebugString();
}

void ExtractTableInt(FzSolver* const solver, FzConstraint* const ct) {
  LOG(FATAL) << "Not implemented: Extract " << ct->DebugString();
}

void ExtractTrueConstraint(FzSolver* const solver, FzConstraint* const ct) {
  LOG(FATAL) << "Not implemented: Extract " << ct->DebugString();
}

void ExtractVarCumulative(FzSolver* const solver, FzConstraint* const ct) {
  LOG(FATAL) << "Not implemented: Extract " << ct->DebugString();
}

void ExtractVariableCumulative(FzSolver* const solver, FzConstraint* const ct) {
  LOG(FATAL) << "Not implemented: Extract " << ct->DebugString();
}

void FzSolver::ExtractConstraint(FzConstraint* const ct) {
  const std::string& type = ct->type;
  if (type == "all_different_int") {
    ExtractAllDifferentInt(this, ct);
  } else if (type == "alldifferent_except_0") {
    ExtractAlldifferentExcept0(this, ct);
  } else if (type == "array_bool_and") {
    ExtractArrayBoolAnd(this, ct);
  } else if (type == "array_bool_element") {
    ExtractArrayIntElement(this, ct);
  } else if (type == "array_bool_or") {
    ExtractArrayBoolOr(this, ct);
  } else if (type == "array_bool_xor") {
    ExtractArrayBoolXor(this, ct);
  } else if (type == "array_int_element") {
    ExtractArrayIntElement(this, ct);
  } else if (type == "array_var_bool_element") {
    ExtractArrayVarIntElement(this, ct);
  } else if (type == "array_var_int_element") {
    ExtractArrayVarIntElement(this, ct);
  } else if (type == "array_var_int_position") {
    ExtractArrayVarIntPosition(this, ct);
  } else if (type == "bool2int") {
    ExtractBool2int(this, ct);
  } else if (type == "bool_and") {
    ExtractBoolAnd(this, ct);
  } else if (type == "bool_clause") {
    ExtractBoolClause(this, ct);
  } else if (type == "bool_eq") {
    ExtractIntEq(this, ct);
  } else if (type == "bool_eq_reif") {
    ExtractIntEqReif(this, ct);
  } else if (type == "bool_ge") {
    ExtractIntGe(this, ct);
  } else if (type == "bool_ge_reif") {
    ExtractIntGeReif(this, ct);
  } else if (type == "bool_gt") {
    ExtractIntGt(this, ct);
  } else if (type == "bool_gt_reif") {
    ExtractIntGtReif(this, ct);
  } else if (type == "bool_le") {
    ExtractIntLe(this, ct);
  } else if (type == "bool_le_reif") {
    ExtractIntLeReif(this, ct);
  } else if (type == "bool_left_imp") {
    ExtractBoolLeftImp(this, ct);
  } else if (type == "bool_lin_eq") {
    ExtractIntLinEq(this, ct);
  } else if (type == "bool_lin_le") {
    ExtractIntLinLe(this, ct);
  } else if (type == "bool_lt") {
    ExtractIntLt(this, ct);
  } else if (type == "bool_lt_reif") {
    ExtractIntLtReif(this, ct);
  } else if (type == "bool_ne") {
    ExtractIntNe(this, ct);
  } else if (type == "bool_ne_reif") {
    ExtractIntNeReif(this, ct);
  } else if (type == "bool_not") {
    ExtractBoolNot(this, ct);
  } else if (type == "bool_or") {
    ExtractBoolOr(this, ct);
  } else if (type == "bool_right_imp") {
    ExtractBoolRightImp(this, ct);
  } else if (type == "bool_xor") {
    ExtractBoolXor(this, ct);
  } else if (type == "circuit") {
    ExtractCircuit(this, ct);
  } else if (type == "count_eq") {
    ExtractCountEq(this, ct);
  } else if (type == "count_geq") {
    ExtractCountGeq(this, ct);
  } else if (type == "count_gt") {
    ExtractCountGt(this, ct);
  } else if (type == "count_leq") {
    ExtractCountLeq(this, ct);
  } else if (type == "count_lt") {
    ExtractCountLt(this, ct);
  } else if (type == "count_neq") {
    ExtractCountNeq(this, ct);
  } else if (type == "count_reif") {
    ExtractCountReif(this, ct);
  } else if (type == "diffn") {
    ExtractDiffn(this, ct);
  } else if (type == "fixed_cumulative") {
    ExtractFixedCumulative(this, ct);
  } else if (type == "global_cardinality") {
    ExtractGlobalCardinality(this, ct);
  } else if (type == "global_cardinality_closed") {
    ExtractGlobalCardinalityClosed(this, ct);
  } else if (type == "global_cardinality_low_up") {
    ExtractGlobalCardinalityLowUp(this, ct);
  } else if (type == "global_cardinality_low_up_closed") {
    ExtractGlobalCardinalityLowUpClosed(this, ct);
  } else if (type == "global_cardinality_old") {
    ExtractGlobalCardinalityOld(this, ct);
  } else if (type == "int_abs") {
    ExtractIntAbs(this, ct);
  } else if (type == "int_div") {
    ExtractIntDiv(this, ct);
  } else if (type == "int_eq") {
    ExtractIntEq(this, ct);
  } else if (type == "int_eq_reif") {
    ExtractIntEqReif(this, ct);
  } else if (type == "int_ge") {
    ExtractIntGe(this, ct);
  } else if (type == "int_ge_reif") {
    ExtractIntGeReif(this, ct);
  } else if (type == "int_gt") {
    ExtractIntGt(this, ct);
  } else if (type == "int_gt_reif") {
    ExtractIntGtReif(this, ct);
  } else if (type == "int_in") {
    ExtractIntIn(this, ct);
  } else if (type == "int_le") {
    ExtractIntLe(this, ct);
  } else if (type == "int_le_reif") {
    ExtractIntLeReif(this, ct);
  } else if (type == "int_lin_eq") {
    ExtractIntLinEq(this, ct);
  } else if (type == "int_lin_eq_reif") {
    ExtractIntLinEqReif(this, ct);
  } else if (type == "int_lin_ge") {
    ExtractIntLinGe(this, ct);
  } else if (type == "int_lin_ge_reif") {
    ExtractIntLinGeReif(this, ct);
  } else if (type == "int_lin_le") {
    ExtractIntLinLe(this, ct);
  } else if (type == "int_lin_le_reif") {
    ExtractIntLinLeReif(this, ct);
  } else if (type == "int_lin_ne") {
    ExtractIntLinNe(this, ct);
  } else if (type == "int_lin_ne_reif") {
    ExtractIntLinNeReif(this, ct);
  } else if (type == "int_lt") {
    ExtractIntLt(this, ct);
  } else if (type == "int_lt_reif") {
    ExtractIntLtReif(this, ct);
  } else if (type == "int_max") {
    ExtractIntMax(this, ct);
  } else if (type == "int_min") {
    ExtractIntMin(this, ct);
  } else if (type == "int_minus") {
    ExtractIntMinus(this, ct);
  } else if (type == "int_mod") {
    ExtractIntMod(this, ct);
  } else if (type == "int_ne") {
    ExtractIntNe(this, ct);
  } else if (type == "int_ne_reif") {
    ExtractIntNeReif(this, ct);
  } else if (type == "int_negate") {
    ExtractIntNegate(this, ct);
  } else if (type == "int_plus") {
    ExtractIntPlus(this, ct);
  } else if (type == "int_times") {
    ExtractIntTimes(this, ct);
  } else if (type == "inverse") {
    ExtractInverse(this, ct);
  } else if (type == "lex_less_bool") {
    ExtractLexLessBool(this, ct);
  } else if (type == "lex_less_int") {
    ExtractLexLessInt(this, ct);
  } else if (type == "lex_lesseq_bool") {
    ExtractLexLesseqBool(this, ct);
  } else if (type == "lex_lesseq_int") {
    ExtractLexLesseqInt(this, ct);
  } else if (type == "maximum_int") {
    ExtractMaximumInt(this, ct);
  } else if (type == "minimum_int") {
    ExtractMinimumInt(this, ct);
  } else if (type == "nvalue") {
    ExtractNvalue(this, ct);
  } else if (type == "regular") {
    ExtractRegular(this, ct);
  } else if (type == "set_in") {
    ExtractSetIn(this, ct);
  } else if (type == "set_in_reif") {
    ExtractSetInReif(this, ct);
  } else if (type == "sliding_sum") {
    ExtractSlidingSum(this, ct);
  } else if (type == "sort") {
    ExtractSort(this, ct);
  } else if (type == "table_bool") {
    ExtractTableBool(this, ct);
  } else if (type == "table_int") {
    ExtractTableInt(this, ct);
  } else if (type == "true_constraint") {
    ExtractTrueConstraint(this, ct);
  } else if (type == "var_cumulative") {
    ExtractVarCumulative(this, ct);
  } else if (type == "variable_cumulative") {
    ExtractVariableCumulative(this, ct);
  }
}
}  // namespace operations_research
