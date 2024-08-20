// ADD HEADER
#include <stdio.h>
#include <time.h>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <locale>
#include <string>
#include <vector>

#include "gtest/gtest.h"
#include "ortools/base/init_google.h"
#include "ortools/knitro/environment.h"
#include "ortools/linear_solver/linear_solver.h"

namespace operations_research {

#define ERROR_RATE 1e-6

#define EXPECT_STATUS(s)                              \
  do {                                                \
    int const status_ = s;                            \
    EXPECT_EQ(0, status_) << "Nonzero return status"; \
  } while (0)

inline bool file_exists(const std::string& name) {
  if (FILE* file = fopen(name.c_str(), "r")) {
    fclose(file);
    return true;
  } else {
    return false;
  }
}

class KnitroGetter {
 public:
  KnitroGetter(MPSolver* solver) : solver_(solver) {}

  // Var getters
  void Num_Var(int* NV) { EXPECT_STATUS(KN_get_number_vars(kc(), NV)); }

  void Var_Lb(MPVariable* x, double* lb) {
    CHECK(solver_->OwnsVariable(x));
    EXPECT_STATUS(KN_get_var_lobnd(kc(), x->index(), lb));
  }

  void Var_Ub(MPVariable* x, double* ub) {
    CHECK(solver_->OwnsVariable(x));
    EXPECT_STATUS(KN_get_var_upbnd(kc(), x->index(), ub));
  }

  void Var_Name(MPVariable* x, char* name, int buffersize) {
    CHECK(solver_->OwnsVariable(x));
    EXPECT_STATUS(KN_get_var_name(kc(), x->index(), buffersize, name));
  }

  void Var_Type(MPVariable* x, int* type) {
    CHECK(solver_->OwnsVariable(x));
    EXPECT_STATUS(KN_get_var_type(kc(), x->index(), type));
  }

  // Cons getters
  void Num_Cons(int* NC) { EXPECT_STATUS(KN_get_number_cons(kc(), NC)); }

  void Con_Lb(MPConstraint* ct, double* lb) {
    EXPECT_STATUS(KN_get_con_lobnd(kc(), ct->index(), lb));
  }

  void Con_Ub(MPConstraint* ct, double* ub) {
    EXPECT_STATUS(KN_get_con_upbnd(kc(), ct->index(), ub));
  }

  void Con_Name(MPConstraint* ct, char* name, int buffersize) {
    EXPECT_STATUS(KN_get_con_name(kc(), ct->index(), buffersize, name));
  }

  void Con_nnz(MPConstraint* ct, int* nnz) {
    EXPECT_STATUS(KN_get_jacobian_nnz_one(kc(), ct->index(), nnz));
  }

  void Con_coef(MPConstraint* ct, int* idx_vars, double* coef) {
    EXPECT_STATUS(
        KN_get_jacobian_values_one(kc(), ct->index(), idx_vars, coef));
  }

  void Con_tot_nnz(KNLONG* nnz) {
    EXPECT_STATUS(KN_get_jacobian_nnz(kc(), nnz));
  }

  void Con_all_coef(int* idx_cons, int* idx_vars, double* coefs) {
    EXPECT_STATUS(KN_get_jacobian_values(kc(), idx_cons, idx_vars, coefs));
  }

  // Obj getters
  void Obj_nb_coef(int* nnz) { EXPECT_STATUS(KN_get_objgrad_nnz(kc(), nnz)); }

  void Obj_all_coef(int* idx_vars, double* coefs) {
    EXPECT_STATUS(KN_get_objgrad_values(kc(), idx_vars, coefs));
  }

  // Param getters
  void Int_Param(int param_id, int* value) {
    EXPECT_STATUS(KN_get_int_param(kc(), param_id, value));
  }

  void Double_Param(int param_id, double* value) {
    EXPECT_STATUS(KN_get_double_param(kc(), param_id, value));
  }

 private:
  MPSolver* solver_;
  KN_context_ptr kc() { return (KN_context_ptr)solver_->underlying_solver(); }
};

#define UNITTEST_INIT_MIP()                                                  \
  MPSolver solver("KNITRO_MIP", MPSolver::KNITRO_MIXED_INTEGER_PROGRAMMING); \
  KnitroGetter getter(&solver)
#define UNITTEST_INIT_LP()                                           \
  MPSolver solver("KNITRO_LP", MPSolver::KNITRO_LINEAR_PROGRAMMING); \
  KnitroGetter getter(&solver)

/*-------------------- TU --------------------*/

// ----- Empty model

/** Unit Test to solve an empty LP */
TEST(KnitroInterface, SolveEmptyLP) {
  UNITTEST_INIT_LP();
  solver.Solve();
  EXPECT_EQ(solver.MutableObjective()->Value(), 0);
}

/** Unit Test to solve an empty MIP */
TEST(KnitroInterface, SolveEmptyMIP) {
  UNITTEST_INIT_MIP();
  solver.Solve();
  EXPECT_EQ(solver.MutableObjective()->Value(), 0);
}

/** Unit Test to write an empty MIP */
TEST(KnitroInterface, WriteEmptyProblem) {
  UNITTEST_INIT_MIP();
  solver.Write("knitro_interface_test_empty");
  // check the file exists
  EXPECT_TRUE(file_exists("knitro_interface_test_empty"));
}

// ----- Modeling functions

/** Unit Test of the method infinity()*/
TEST(KnitroInterface, GetKnitroInfinityValue) {
  UNITTEST_INIT_LP();
  EXPECT_EQ(solver.solver_infinity(), KN_INFINITY);
}

/** Unit Test of the method AddVariable()*/
TEST(KnitroInterface, AddVariableIntoProblem) {
  UNITTEST_INIT_LP();
  MPVariable* const x = solver.MakeNumVar(0, 10, "x");
  solver.Solve();
  double lb, ub;
  char name[20];
  getter.Var_Lb(x, &lb);
  getter.Var_Ub(x, &ub);
  getter.Var_Name(x, name, 20);
  EXPECT_EQ(lb, 0);
  EXPECT_EQ(ub, 10);
  EXPECT_STREQ(name, "x");
}

/** Unit Test of the method AddRowConstraint()*/
TEST(KnitroInterface, AddRowConstraintIntoProblem) {
  UNITTEST_INIT_LP();
  MPConstraint* const ct = solver.MakeRowConstraint(0, 10, "ct");
  solver.Solve();
  double lb, ub;
  char name[20];
  getter.Con_Lb(ct, &lb);
  getter.Con_Ub(ct, &ub);
  getter.Con_Name(ct, name, 20);
  EXPECT_EQ(lb, 0);
  EXPECT_EQ(ub, 10);
  EXPECT_STREQ(name, "ct");
}

/** Unit Test of the method SetCoefficient()*/
TEST(KnitroInterface, SetCoefficientInConstraint) {
  UNITTEST_INIT_LP();
  MPVariable* const x = solver.MakeNumVar(0, 10, "x");
  MPConstraint* const ct = solver.MakeRowConstraint(0, 10, "ct");
  ct->SetCoefficient(x, 2);
  solver.Solve();
  int idx_con, idx_var;
  double coef;
  getter.Con_all_coef(&idx_con, &idx_var, &coef);
  EXPECT_EQ(x->index(), idx_var);
  EXPECT_EQ(ct->index(), idx_con);
  EXPECT_EQ(coef, 2);
  ct->SetCoefficient(x, 1);
  getter.Con_all_coef(&idx_con, &idx_var, &coef);
  EXPECT_EQ(x->index(), idx_var);
  EXPECT_EQ(ct->index(), idx_con);
  EXPECT_EQ(coef, 1);
}

/** Unit Test to check variable type in Knitro model*/
TEST(KnitroInterface, CheckVarType) {
  UNITTEST_INIT_MIP();
  MPVariable* const x = solver.MakeNumVar(0, 10, "x");
  MPVariable* const y = solver.MakeIntVar(0, 10, "y");
  MPVariable* const z = solver.MakeBoolVar("z");
  solver.Solve();
  int value;
  getter.Var_Type(x, &value);
  EXPECT_EQ(value, KN_VARTYPE_CONTINUOUS);
  getter.Var_Type(y, &value);
  EXPECT_EQ(value, KN_VARTYPE_INTEGER);
  getter.Var_Type(z, &value);
  EXPECT_EQ(value, KN_VARTYPE_BINARY);
}

/** Unit Test to change variable type from continuous to integer*/
TEST(KnitroInterface, ChangeVarTypeIntoInteger) {
  UNITTEST_INIT_MIP();
  MPVariable* const x = solver.MakeNumVar(0, 10, "x");
  MPVariable* const y = solver.MakeIntVar(0, 10, "y");
  solver.Solve();
  int value;
  getter.Var_Type(x, &value);
  EXPECT_EQ(value, KN_VARTYPE_CONTINUOUS);
  getter.Var_Type(y, &value);
  EXPECT_EQ(value, KN_VARTYPE_INTEGER);
  x->SetInteger(true);
  y->SetInteger(false);
  solver.Solve();
  getter.Var_Type(x, &value);
  EXPECT_EQ(value, KN_VARTYPE_INTEGER);
  getter.Var_Type(y, &value);
  EXPECT_EQ(value, KN_VARTYPE_CONTINUOUS);
}

/** Unit Test to change variable type from continuous to integer in LP*/
TEST(KnitroInterface, ChangeVarTypeIntoIntegerInLP) {
  UNITTEST_INIT_LP();
  MPVariable* const x = solver.MakeNumVar(0, 10, "x");
  MPVariable* const y = solver.MakeNumVar(0, 10, "y");
  x->SetInteger(true);
  solver.Solve();
  int value;
  getter.Var_Type(x, &value);
  EXPECT_EQ(value, KN_VARTYPE_CONTINUOUS);
  getter.Var_Type(y, &value);
  EXPECT_EQ(value, KN_VARTYPE_CONTINUOUS);
  x->SetInteger(true);
  y->SetInteger(true);
  getter.Var_Type(x, &value);
  EXPECT_EQ(value, KN_VARTYPE_CONTINUOUS);
  getter.Var_Type(y, &value);
  EXPECT_EQ(value, KN_VARTYPE_CONTINUOUS);
  solver.Solve();
  getter.Var_Type(x, &value);
  EXPECT_EQ(value, KN_VARTYPE_CONTINUOUS);
  getter.Var_Type(y, &value);
  EXPECT_EQ(value, KN_VARTYPE_CONTINUOUS);
}

/** Unit Test of the method SetVariableBounds()*/
TEST(KnitroInterface, ChangeVariableBounds) {
  UNITTEST_INIT_LP();
  MPVariable* const x = solver.MakeNumVar(0, 1, "x");
  x->SetBounds(-10, 10);
  solver.Solve();
  double lb, ub;
  getter.Var_Lb(x, &lb);
  getter.Var_Ub(x, &ub);
  EXPECT_EQ(lb, -10);
  EXPECT_EQ(ub, 10);
  x->SetBounds(-1, 1);
  getter.Var_Lb(x, &lb);
  getter.Var_Ub(x, &ub);
  EXPECT_EQ(lb, -1);
  EXPECT_EQ(ub, 1);
}

/** Unit Test of the method ClearConstraint()*/
TEST(KnitroInterface, ClearAConstraint) {
  UNITTEST_INIT_LP();
  MPVariable* const x = solver.MakeNumVar(0, 10, "x");
  MPConstraint* const ct = solver.MakeRowConstraint(0, 10, "ct");
  ct->SetCoefficient(x, 2);
  KNLONG nnz;
  getter.Con_tot_nnz(&nnz);
  // The Constraint has not been extracted yet
  EXPECT_EQ(nnz, 0);
  solver.Solve();
  getter.Con_tot_nnz(&nnz);
  EXPECT_EQ(nnz, 1);
  ct->Clear();
  getter.Con_tot_nnz(&nnz);
  EXPECT_EQ(nnz, 0);
}

/** Unit Test of the method SetObjectiveCoefficient()*/
TEST(KnitroInterface, AddObjectiveCoefficient) {
  UNITTEST_INIT_LP();
  MPVariable* const x = solver.MakeNumVar(0, 10, "x");
  MPObjective* const obj = solver.MutableObjective();
  obj->SetCoefficient(x, 1);
  solver.Solve();
  int idx_var;
  double coef;
  getter.Obj_all_coef(&idx_var, &coef);
  EXPECT_EQ(x->index(), idx_var);
  EXPECT_EQ(coef, 1);
}

/** Unit Test of the method SetOptimizationDirection()*/
TEST(KnitroInterface, ChangeOptimizationDirection) {
  UNITTEST_INIT_LP();
  MPVariable* const x = solver.MakeNumVar(0, 1, "x");
  MPObjective* const obj = solver.MutableObjective();
  obj->SetCoefficient(x, 1);
  obj->SetMaximization();
  solver.Solve();
  EXPECT_EQ(obj->Value(), 1);
  obj->SetMinimization();
  solver.Solve();
  EXPECT_EQ(obj->Value(), 0);
}

/** Unit Test of the method SetObjectiveOffset()*/
TEST(KnitroInterface, AddOffsetToObjective) {
  UNITTEST_INIT_LP();
  MPVariable* const x = solver.MakeNumVar(0, 10, "x");
  MPObjective* const obj = solver.MutableObjective();
  obj->SetCoefficient(x, 1);
  solver.Solve();
  EXPECT_EQ(obj->Value(), 0);
  obj->SetOffset(1);
  solver.Solve();
  EXPECT_EQ(obj->Value(), 1);
}

/** Unit Test of the method ClearObjective()*/
TEST(KnitroInterface, ClearObjective) {
  UNITTEST_INIT_LP();
  MPVariable* const x = solver.MakeNumVar(0, 1, "x");
  MPObjective* const obj = solver.MutableObjective();
  obj->SetCoefficient(x, 1);
  int nnz;
  getter.Obj_nb_coef(&nnz);
  // The objective coefficient has not been extracted yet
  EXPECT_EQ(nnz, 0);
  solver.Solve();
  getter.Obj_nb_coef(&nnz);
  EXPECT_EQ(nnz, 1);
  obj->Clear();
  getter.Obj_nb_coef(&nnz);
  EXPECT_EQ(nnz, 0);
}

/** Unit Test of the method Reset()*/
TEST(KnitroInterface, ResetAProblem) {
  UNITTEST_INIT_LP();
  MPVariable* const x = solver.MakeNumVar(0, 1, "x");
  MPVariable* const y = solver.MakeNumVar(0, 1, "y");
  MPConstraint* const c1 = solver.MakeRowConstraint(0, 3, "c1");
  c1->SetCoefficient(x, .5);
  c1->SetCoefficient(y, 1);
  MPConstraint* const c2 = solver.MakeRowConstraint(0, 3, "c2");
  c2->SetCoefficient(x, 1);
  c2->SetCoefficient(y, 1);
  MPObjective* const obj = solver.MutableObjective();
  obj->SetCoefficient(x, 1);
  obj->SetCoefficient(y, -1);

  solver.Solve();  // to extract the model
  int nb_vars, nb_cons, nnz;
  getter.Num_Var(&nb_vars);
  getter.Num_Cons(&nb_cons);
  getter.Obj_nb_coef(&nnz);
  EXPECT_EQ(nb_vars, 2);
  EXPECT_EQ(nb_cons, 2);
  EXPECT_EQ(nnz, 2);

  solver.Reset();
  getter.Num_Var(&nb_vars);
  getter.Num_Cons(&nb_cons);
  getter.Obj_nb_coef(&nnz);
  EXPECT_EQ(nb_vars, 0);
  EXPECT_EQ(nb_cons, 0);
  EXPECT_EQ(nnz, 0);
}

// ----- Test Param

/** Unit Test of the method SetScaling()*/
TEST(KnitroInterface, SetScalingParam) {
  UNITTEST_INIT_MIP();
  MPSolverParameters params;
  params.SetIntegerParam(MPSolverParameters::SCALING,
                         MPSolverParameters::SCALING_OFF);
  solver.Solve(params);
  int value;
  getter.Int_Param(KN_PARAM_LINSOLVER_SCALING, &value);
  EXPECT_EQ(value, KN_LINSOLVER_SCALING_NONE);
  params.SetIntegerParam(MPSolverParameters::SCALING,
                         MPSolverParameters::SCALING_ON);
  solver.Solve(params);
  getter.Int_Param(KN_PARAM_LINSOLVER_SCALING, &value);
  EXPECT_EQ(value, KN_LINSOLVER_SCALING_ALWAYS);
}

/** Unit Test of the method SetRelativeMipGap()*/
TEST(KnitroInterface, ChangeRelativeMipGapinMIP) {
  UNITTEST_INIT_MIP();
  MPSolverParameters params;
  params.SetDoubleParam(MPSolverParameters::RELATIVE_MIP_GAP, 0.5);
  solver.Solve(params);
  double value;
  getter.Double_Param(KN_PARAM_MIP_OPTGAPREL, &value);
  EXPECT_EQ(value, .5);
  params.SetDoubleParam(MPSolverParameters::RELATIVE_MIP_GAP, 1e-8);
  solver.Solve(params);
  getter.Double_Param(KN_PARAM_MIP_OPTGAPREL, &value);
  EXPECT_EQ(value, 1e-8);
}

/** Unit Test of the method SetRelativeMipGap()*/
TEST(KnitroInterface, ChangeRelativeMipGapinLP) {
  UNITTEST_INIT_LP();
  MPSolverParameters params;
  params.SetDoubleParam(MPSolverParameters::RELATIVE_MIP_GAP, 0.5);
  solver.Solve(params);
  double value;
  getter.Double_Param(KN_PARAM_MIP_OPTGAPREL, &value);
  EXPECT_EQ(value, 1e-4);
}

/** Unit Test of the method SetPresolveMode()*/
TEST(KnitroInterface, SetPresolveModeParam) {
  UNITTEST_INIT_MIP();
  MPSolverParameters params;
  // Try with invalid value
  params.SetIntegerParam(MPSolverParameters::PRESOLVE, 2);
  solver.Solve(params);
  int value;
  getter.Int_Param(KN_PARAM_PRESOLVE, &value);
  // expect the default value
  EXPECT_EQ(value, KN_PRESOLVE_YES);
  // Try with presolve off
  params.SetIntegerParam(MPSolverParameters::PRESOLVE,
                         MPSolverParameters::PRESOLVE_OFF);
  solver.Solve(params);
  getter.Int_Param(KN_PARAM_PRESOLVE, &value);
  EXPECT_EQ(value, KN_PRESOLVE_NO);
  // Try with presolve on
  params.SetIntegerParam(MPSolverParameters::PRESOLVE,
                         MPSolverParameters::PRESOLVE_ON);
  solver.Solve(params);
  getter.Int_Param(KN_PARAM_PRESOLVE, &value);
  EXPECT_EQ(value, KN_PRESOLVE_YES);
}

/** Unit Test of the method AddSolutionHintToOptimizer()*/
TEST(KnitroInterface, AddSolutionHintToOptimizer) {
  UNITTEST_INIT_LP();
  double infinity = solver.infinity();
  MPVariable* const x = solver.MakeNumVar(0, infinity, "x");
  MPVariable* const y = solver.MakeNumVar(0, infinity, "y");
  MPConstraint* const c1 = solver.MakeRowConstraint(0, 1, "c1");
  c1->SetCoefficient(x, 1);
  c1->SetCoefficient(y, 1);
  MPObjective* const obj = solver.MutableObjective();
  obj->SetCoefficient(x, 1);
  obj->SetMaximization();
  std::vector<std::pair<const MPVariable*, double> > hint;
  hint.push_back(std::make_pair(x, 1.));
  hint.push_back(std::make_pair(y, 0.));
  solver.SetHint(hint);
  solver.Solve();
  EXPECT_NEAR(obj->Value(), 1, ERROR_RATE);
  EXPECT_NEAR(x->solution_value(), 1, ERROR_RATE);
  EXPECT_NEAR(y->solution_value(), 0, ERROR_RATE);
}

/** Unit Test of the method SetSolverSpecificParametersAsString()*/
TEST(KnitroInterface, SetSolverSpecificParametersAsString) {
  UNITTEST_INIT_MIP();
  EXPECT_TRUE(solver.SetSolverSpecificParametersAsString(""));
  EXPECT_FALSE(solver.SetSolverSpecificParametersAsString("blabla 5 "));
  solver.SetSolverSpecificParametersAsString(
      "KN_PARAM_FEASTOL 1e-8 KN_PARAM_LINSOLVER_SCALING 1 ");
  solver.Solve();
  int value;
  getter.Int_Param(KN_PARAM_LINSOLVER_SCALING, &value);
  EXPECT_EQ(value, 1);
  double value2;
  getter.Double_Param(KN_PARAM_FEASTOL, &value2);
  EXPECT_EQ(value2, 1e-8);
}

/** Unit Test of the method SetLpAlgorithm()*/
TEST(KnitroInterface, SetLpAlgorithm) {
  UNITTEST_INIT_LP();
  MPSolverParameters params;
  params.SetIntegerParam(MPSolverParameters::LP_ALGORITHM,
                         MPSolverParameters::PRIMAL);
  solver.Solve(params);
  int value;
  getter.Int_Param(KN_PARAM_ACT_LPALG, &value);
  EXPECT_EQ(value, KN_ACT_LPALG_PRIMAL);
  params.SetIntegerParam(MPSolverParameters::LP_ALGORITHM,
                         MPSolverParameters::DUAL);
  solver.Solve(params);
  getter.Int_Param(KN_PARAM_ACT_LPALG, &value);
  EXPECT_EQ(value, KN_ACT_LPALG_DUAL);
  params.SetIntegerParam(MPSolverParameters::LP_ALGORITHM,
                         MPSolverParameters::BARRIER);
  solver.Solve(params);
  getter.Int_Param(KN_PARAM_ACT_LPALG, &value);
  EXPECT_EQ(value, KN_ACT_LPALG_BARRIER);
  params.SetIntegerParam(MPSolverParameters::LP_ALGORITHM, -1000);
  solver.Solve(params);
  getter.Int_Param(KN_PARAM_ACT_LPALG, &value);
  EXPECT_EQ(value, KN_ACT_LPALG_DEFAULT);
}

/** Unit Test of the method SetPrimalTolerance()*/
TEST(KnitroInterface, SetPrimalToleranceParam) {
  UNITTEST_INIT_LP();
  MPSolverParameters params;
  params.SetDoubleParam(MPSolverParameters::PRIMAL_TOLERANCE, 0.5);
  solver.Solve(params);
  double value;
  getter.Double_Param(KN_PARAM_FEASTOL, &value);
  EXPECT_EQ(value, .5);
  params.SetDoubleParam(MPSolverParameters::PRIMAL_TOLERANCE, 1e-8);
  solver.Solve(params);
  getter.Double_Param(KN_PARAM_FEASTOL, &value);
  EXPECT_EQ(value, 1e-8);
}

/** Unit Test of the method SetDualTolerance()*/
TEST(KnitroInterface, SetDualToleranceParam) {
  UNITTEST_INIT_LP();
  MPSolverParameters params;
  params.SetDoubleParam(MPSolverParameters::DUAL_TOLERANCE, 0.5);
  solver.Solve(params);
  double value;
  getter.Double_Param(KN_PARAM_OPTTOL, &value);
  EXPECT_EQ(value, .5);
  params.SetDoubleParam(MPSolverParameters::DUAL_TOLERANCE, 1e-8);
  solver.Solve(params);
  getter.Double_Param(KN_PARAM_OPTTOL, &value);
  EXPECT_EQ(value, 1e-8);
}

/** Unit Test of the method SetNumThreads()*/
TEST(KnitroInterface, SetNumThreads) {
  UNITTEST_INIT_MIP();
  auto status = solver.SetNumThreads(4);
  solver.Solve();
  int value;
  getter.Int_Param(KN_PARAM_NUMTHREADS, &value);
  EXPECT_EQ(value, 4);
}

/** Unit Test with time limit*/
TEST(KnitroInterface, SetProblemTimeLimit) {
  UNITTEST_INIT_LP();
  solver.set_time_limit(2024000);
  solver.Solve();
  double value;
  getter.Double_Param(KN_PARAM_MAXTIMECPU, &value);
  EXPECT_EQ(value, 2024);
}

/**
 * Unit Test of the method BranchingPriorityChangedForVariable()
 * for LP problem (should not work)
 */
TEST(KnitroInterface, BranchingPriorityChangedForVariableLP) {
  UNITTEST_INIT_LP();
  MPVariable* const x = solver.MakeNumVar(0, 1, "x");
  x->SetBranchingPriority(10);
  // The variable priority changed in the OR-Tools model but not in the Knitro
  // model
  EXPECT_EQ(x->branching_priority(), 10);
}

/** Unit Test of the method underlying_solver()*/
TEST(KnitroInterface, GetUnderlyingSolverPtr) {
  UNITTEST_INIT_LP();
  auto ptr = solver.underlying_solver();
  EXPECT_NE(ptr, nullptr);
}

// ----- Getting post-solve informations

/** Unit Test of the method nodes() in MIP*/
TEST(KnitroInterface, GetNbNodesInMIP) {
  UNITTEST_INIT_MIP();
  EXPECT_EQ(solver.nodes(), MPSolverInterface::kUnknownNumberOfNodes);
  solver.Solve();
  EXPECT_NE(solver.nodes(), MPSolverInterface::kUnknownNumberOfNodes);
}

/** Unit Test of the method nodes() in LP*/
TEST(KnitroInterface, GetNbNodesInLP) {
  UNITTEST_INIT_LP();
  EXPECT_EQ(solver.nodes(), MPSolverInterface::kUnknownNumberOfNodes);
  solver.Solve();
  EXPECT_EQ(solver.nodes(), MPSolverInterface::kUnknownNumberOfNodes);
}

/** Unit Test of the method iterations()*/
TEST(KnitroInterface, GetNbIter) {
  UNITTEST_INIT_MIP();
  EXPECT_EQ(solver.iterations(), MPSolverInterface::kUnknownNumberOfIterations);
  solver.Solve();
  EXPECT_NE(solver.iterations(), MPSolverInterface::kUnknownNumberOfIterations);
}

/*-------------------- TF --------------------*/

/**
 * Writes the following linear problem
 * using KnitroInterface Write function
 *    max  x + 2y
 *    st. 3x - 4y >= 10
 *        2x + 3y <= 18
 *         x,   y \in R+
 *
 * Then loads it in a Knitro model and solves it
 *
 * TODO : For next version of Knitro : Add Ranges,
 * Constants and different bounds for variables to
 * be written in mps file
 */
TEST(KnitroInterface, WriteAndLoadModel) {
  // Write model using OR-Tools
  UNITTEST_INIT_LP();
  std::string version = solver.SolverVersion();
  version.resize(30);
  EXPECT_STREQ(version.c_str(), "Knitro library version Knitro ");
  const double infinity = solver.solver_infinity();

  MPVariable* const x = solver.MakeNumVar(0.0, infinity, "x");
  MPVariable* const y = solver.MakeNumVar(0.0, infinity, "y");

  MPConstraint* const c1 = solver.MakeRowConstraint(10.0, infinity, "c1");
  c1->SetCoefficient(x, 3);
  c1->SetCoefficient(y, -4);
  MPConstraint* const c2 = solver.MakeRowConstraint(-infinity, 18.0, "c2");
  c2->SetCoefficient(x, 2);
  c2->SetCoefficient(y, 3);

  MPObjective* const obj = solver.MutableObjective();
  obj->SetCoefficient(x, 1);
  obj->SetCoefficient(y, 2);
  obj->SetMaximization();

  solver.Write("knitro_interface_test_LP_model");
  EXPECT_TRUE(file_exists("knitro_interface_test_LP_model"));

  // Check variable x
  double lb, ub;
  char name[20];
  getter.Var_Lb(x, &lb);
  getter.Var_Ub(x, &ub);
  getter.Var_Name(x, name, 20);
  EXPECT_EQ(lb, 0.0);
  EXPECT_EQ(ub, infinity);
  EXPECT_STREQ(name, "x");

  // Check constraint c1
  getter.Con_Lb(c1, &lb);
  getter.Con_Ub(c1, &ub);
  getter.Con_Name(c1, name, 20);
  EXPECT_EQ(lb, 10.0);
  EXPECT_EQ(ub, infinity);
  EXPECT_STREQ(name, "c1");

  // Load directly from Knitro model
  KN_context* kc;
  KN_new(&kc);
  KN_load_mps_file(kc, "knitro_interface_test_LP_model");
  KN_set_int_param(kc, KN_PARAM_OUTLEV, KN_OUTLEV_NONE);
  // Check variables
  double kc_lb[2], kc_ub[2];
  KN_get_var_lobnds_all(kc, kc_lb);
  KN_get_var_upbnds_all(kc, kc_ub);
  EXPECT_EQ(kc_lb[0], 0);
  EXPECT_EQ(kc_lb[1], 0);
  EXPECT_EQ(kc_ub[0], KN_INFINITY);
  EXPECT_EQ(kc_ub[1], KN_INFINITY);
  char* names[2];
  names[0] = new char[20];
  names[1] = new char[20];
  KN_get_var_names_all(kc, 20, names);
  EXPECT_STREQ(names[0], "x");
  EXPECT_STREQ(names[1], "y");
  // Check constraints
  KN_get_con_lobnds_all(kc, kc_lb);
  KN_get_con_upbnds_all(kc, kc_ub);
  EXPECT_EQ(kc_lb[0], 10);
  EXPECT_EQ(kc_lb[1], -KN_INFINITY);
  EXPECT_EQ(kc_ub[0], KN_INFINITY);
  EXPECT_EQ(kc_ub[1], 18);
  KN_get_con_names_all(kc, 20, names);
  EXPECT_STREQ(names[0], "c1");
  EXPECT_STREQ(names[1], "c2");
  // Check everything else by solving the lp
  double objSol;
  int nStatus = KN_solve(kc);
  double kc_x[2];
  KN_get_solution(kc, &nStatus, &objSol, kc_x, NULL);
  EXPECT_NEAR(kc_x[0], 6, ERROR_RATE);
  EXPECT_NEAR(kc_x[1], 2, ERROR_RATE);
  EXPECT_NEAR(objSol, 10, ERROR_RATE);

  delete[] names[0];
  delete[] names[1];
  KN_free(&kc);
}

/**
 * Solves the following LP
 *    max   x + 2y + 2
 *    st.  -x +  y <= 1
 *         3x + 2y <= 12
 *         2x + 3y <= 12
 *          x ,  y \in R+
 */
TEST(KnitroInterface, SolveLP) {
  UNITTEST_INIT_LP();
  EXPECT_FALSE(solver.IsMIP());
  double inf = solver.infinity();
  MPVariable* x = solver.MakeNumVar(0, inf, "x");
  MPVariable* y = solver.MakeNumVar(0, inf, "y");
  MPObjective* obj = solver.MutableObjective();
  obj->SetCoefficient(x, 1);
  obj->SetCoefficient(y, 2);
  obj->SetOffset(2);
  obj->SetMaximization();
  MPConstraint* c1 = solver.MakeRowConstraint(-inf, 1);
  c1->SetCoefficient(x, -1);
  c1->SetCoefficient(y, 1);
  MPConstraint* c2 = solver.MakeRowConstraint(-inf, 12);
  c2->SetCoefficient(x, 3);
  c2->SetCoefficient(y, 2);
  MPConstraint* c3 = solver.MakeRowConstraint(-inf, 12);
  c3->SetCoefficient(x, 2);
  c3->SetCoefficient(y, 3);
  solver.Solve();

  EXPECT_NEAR(obj->Value(), 9.4, ERROR_RATE);
  EXPECT_NEAR(x->solution_value(), 1.8, ERROR_RATE);
  EXPECT_NEAR(y->solution_value(), 2.8, ERROR_RATE);
  EXPECT_NEAR(x->reduced_cost(), 0, ERROR_RATE);
  EXPECT_NEAR(y->reduced_cost(), 0, ERROR_RATE);
  EXPECT_NEAR(c1->dual_value(), 0.2, ERROR_RATE);
  EXPECT_NEAR(c2->dual_value(), 0, ERROR_RATE);
  EXPECT_NEAR(c3->dual_value(), 0.6, ERROR_RATE);
}

/**
 * Solves the following MIP
 *    max  x -  y + 5z
 *    st.  x + 2y -  z <= 19.5
 *         x +  y +  z >= 3.14
 *         x           <= 10
 *              y +  z <= 6
 *         x,   y,   z \in R+
 */
TEST(KnitroInterface, SolveMIP) {
  UNITTEST_INIT_MIP();
  EXPECT_TRUE(solver.IsMIP());
  const double infinity = solver.infinity();
  // x and y are integer non-negative variables.
  MPVariable* const x = solver.MakeNumVar(0.0, infinity, "x");
  MPVariable* const y = solver.MakeNumVar(0.0, infinity, "y");
  MPVariable* const z = solver.MakeIntVar(0.0, infinity, "z");

  // x + 2 * y - z <= 19.5
  MPConstraint* const c0 = solver.MakeRowConstraint(-infinity, 19.5, "c0");
  c0->SetCoefficient(x, 1);
  c0->SetCoefficient(y, 2);
  c0->SetCoefficient(z, -1);

  // x + y + z >= 3.14
  MPConstraint* const c1 = solver.MakeRowConstraint(3.14, infinity, "c1");
  c1->SetCoefficient(x, 1);
  c1->SetCoefficient(y, 1);
  c1->SetCoefficient(z, 1);

  // x <= 10.
  MPConstraint* const c2 = solver.MakeRowConstraint(-infinity, 10.0, "c2");
  c2->SetCoefficient(x, 1);
  c2->SetCoefficient(y, 0);
  c2->SetCoefficient(z, 0);

  // y + z <= 6.
  MPConstraint* const c3 = solver.MakeRowConstraint(-infinity, 6.0, "c3");
  c3->SetCoefficient(x, 0);
  c3->SetCoefficient(y, 1);
  c3->SetCoefficient(z, 1);

  // Maximize x - y + 5 * z.
  MPObjective* const objective = solver.MutableObjective();
  objective->SetCoefficient(x, 1);
  objective->SetCoefficient(y, -1);
  objective->SetCoefficient(z, 5);
  objective->SetMaximization();

  EXPECT_EQ(solver.nodes(), MPSolverInterface::kUnknownNumberOfNodes);
  EXPECT_EQ(solver.iterations(), MPSolverInterface::kUnknownNumberOfIterations);

  const MPSolver::ResultStatus result_status = solver.Solve();
  EXPECT_NEAR(objective->Value(), 40, ERROR_RATE);
  EXPECT_NEAR(x->solution_value(), 10, ERROR_RATE);
  EXPECT_NEAR(y->solution_value(), 0, ERROR_RATE);
  EXPECT_NEAR(z->solution_value(), 6, ERROR_RATE);

  // Just Check that the methods return something

  EXPECT_NE(solver.nodes(), MPSolverInterface::kUnknownNumberOfNodes);
  EXPECT_NE(solver.iterations(), MPSolverInterface::kUnknownNumberOfIterations);
}

/**
 * Find a feasible solution for the following LP
 *    max    x   +   2y
 *    st.    x          >= 0
 *          -x   +  .5y <= .5
 *        -.5x   +  .5y <= .75
 *        -.5x   +    y <= 2
 *                    y <= 3
 *      -.001x   +    y >= -.01
 * within few iterations
 */
TEST(KnitroInterface, FeasibleSolLP) {
  UNITTEST_INIT_LP();
  const double infinity = solver.infinity();
  MPVariable* const x = solver.MakeNumVar(-infinity, infinity, "x");
  MPVariable* const y = solver.MakeNumVar(-infinity, infinity, "y");

  MPConstraint* const c0 = solver.MakeRowConstraint(0, infinity, "c0");
  c0->SetCoefficient(x, 1);
  MPConstraint* const c1 = solver.MakeRowConstraint(-infinity, .5, "c1");
  c1->SetCoefficient(x, -1);
  c1->SetCoefficient(y, .5);
  MPConstraint* const c2 = solver.MakeRowConstraint(-infinity, .75, "c2");
  c2->SetCoefficient(x, -.5);
  c2->SetCoefficient(y, .5);
  MPConstraint* const c3 = solver.MakeRowConstraint(-infinity, 2, "c3");
  c3->SetCoefficient(x, -.5);
  c3->SetCoefficient(y, 1);
  MPConstraint* const c4 = solver.MakeRowConstraint(-infinity, 3, "c4");
  c4->SetCoefficient(y, 1);
  MPConstraint* const c5 = solver.MakeRowConstraint(-.01, infinity, "c5");
  c5->SetCoefficient(x, -.001);
  c5->SetCoefficient(y, 1);

  MPObjective* const objective = solver.MutableObjective();
  objective->SetCoefficient(x, 1);
  objective->SetCoefficient(y, 2);
  objective->SetMaximization();

  // setting max iter limit
  solver.SetSolverSpecificParametersAsString("KN_PARAM_MAXIT 4 ");

  const MPSolver::ResultStatus result_status = solver.Solve();
  EXPECT_EQ(result_status, MPSolver::ResultStatus::FEASIBLE);
}

/**
 * Try to solve the unbounded problem
 *    max    x
 *    st.    x >= 0
 */
TEST(KnitroInterface, UnboundedLP) {
  UNITTEST_INIT_LP();
  const double infinity = solver.infinity();
  MPVariable* const x = solver.MakeNumVar(0, infinity, "x");
  MPObjective* const objective = solver.MutableObjective();
  objective->SetCoefficient(x, 1);
  objective->SetMaximization();
  const MPSolver::ResultStatus result_status = solver.Solve();
  EXPECT_EQ(result_status, MPSolver::ResultStatus::UNBOUNDED);
}

/**
 * Try to solve the infeasible problem
 *    max   x
 *    st.   x >= +1
 *          x <= -1
 */
TEST(KnitroInterface, InfeasibleLP) {
  UNITTEST_INIT_LP();
  const double infinity = solver.infinity();
  MPVariable* const x = solver.MakeNumVar(-infinity, infinity, "x");
  MPConstraint* const cp = solver.MakeRowConstraint(1, infinity, "cp");
  cp->SetCoefficient(x, 1);
  MPConstraint* const cm = solver.MakeRowConstraint(-infinity, -1, "cm");
  cm->SetCoefficient(x, 1);
  const MPSolver::ResultStatus result_status = solver.Solve();
  EXPECT_EQ(result_status, MPSolver::ResultStatus::INFEASIBLE);
}

/**
 * Checks that KnitroInterface convert correctly the infinity values
 *    max  x + 2y
 *    st.  x + 4y >= -8
 *         x + 4y <= 17
 *        -x +  y >= -2
 *        -x +  y <=  3
 *         x,   y >= 0
 */
TEST(KnitroInterface, SupportInfinity) {
  UNITTEST_INIT_LP();
  double infinity = solver.infinity();
  MPVariable* const x = solver.MakeNumVar(0, infinity, "x");
  MPVariable* const y = solver.MakeNumVar(0, infinity, "y");

  MPConstraint* const c1 = solver.MakeRowConstraint(-8, 17, "c1");
  c1->SetCoefficient(x, 1);
  c1->SetCoefficient(y, 4);
  MPConstraint* const c2 = solver.MakeRowConstraint(-2, 3, "c2");
  c2->SetCoefficient(x, -1);
  c2->SetCoefficient(y, 1);

  MPObjective* const obj = solver.MutableObjective();
  obj->SetCoefficient(x, 1);
  obj->SetCoefficient(y, 2);
  obj->SetMaximization();

  solver.Solve();
  EXPECT_NEAR(11, obj->Value(), ERROR_RATE);
  EXPECT_NEAR(5, x->solution_value(), ERROR_RATE);
  EXPECT_NEAR(3, y->solution_value(), ERROR_RATE);

  // change boundaries to infinity
  x->SetBounds(-infinity, infinity);
  c2->SetBounds(-2, infinity);
  solver.Solve();
  EXPECT_NEAR(11, obj->Value(), ERROR_RATE);
  EXPECT_NEAR(5, x->solution_value(), ERROR_RATE);
  EXPECT_NEAR(3, y->solution_value(), ERROR_RATE);
}

/**
 * Solves a LP without counstraints
 *    max x + y + z
 *    st. x,  y,  z >= 0
 *        x,  y,  z <= 1
 */
TEST(KnitroInterface, JustVarProb) {
  UNITTEST_INIT_LP();
  std::vector<MPVariable*> x;
  solver.MakeNumVarArray(3, 0, 1, "x", &x);
  MPObjective* const obj = solver.MutableObjective();
  obj->SetCoefficient(x[0], 1);
  obj->SetCoefficient(x[1], 1);
  obj->SetCoefficient(x[2], 1);
  obj->SetMaximization();

  solver.Solve();
  EXPECT_NEAR(obj->Value(), 3, ERROR_RATE);
}

/**
 * MIP without objective
 * It solves the [3x3 magic square
 * problem](https://en.wikipedia.org/wiki/Magic_square) by finding feasible
 * solution
 */
TEST(KnitroInterface, FindFeasSol) {
  // find a 3x3 non trivial magic square config
  UNITTEST_INIT_MIP();
  double infinity = solver.infinity();
  std::vector<MPVariable*> x;
  solver.MakeIntVarArray(9, 1, infinity, "x", &x);

  std::vector<MPVariable*> diff;
  solver.MakeBoolVarArray(36, "diff", &diff);

  int debut[] = {0, 8, 15, 21, 26, 30, 33, 35};
  for (int i = 0; i < 9; i++) {
    for (int j = i + 1; j < 9; j++) {
      MPConstraint* const d =
          solver.MakeRowConstraint(1.0, 8.0, "dl" + 10 * i + j);
      d->SetCoefficient(x[i], 1);
      d->SetCoefficient(x[j], -1);
      d->SetCoefficient(diff[debut[i] + j - 1 - i], 9.0);
    }
  }

  int ref[] = {0, 1, 2};
  int comp[][3] = {{3, 4, 5}, {6, 7, 8}, {0, 3, 6}, {7, 1, 4},
                   {5, 8, 2}, {0, 4, 8}, {4, 6, 2}};

  for (auto e : comp) {
    MPConstraint* const d = solver.MakeRowConstraint(0, 0, "eq");
    for (int i = 0; i < 3; i++) {
      if (ref[i] != e[i]) {
        d->SetCoefficient(x[ref[i]], 1);
        d->SetCoefficient(x[e[i]], -1);
      }
    }
  }

  solver.Solve();
  for (int i = 0; i < 9; i++) {
    for (int j = i + 1; j < 9; j++) {
      EXPECT_NE(x[i]->solution_value(), x[j]->solution_value());
    }
  }
  int val = x[ref[0]]->solution_value() + x[ref[1]]->solution_value() +
            x[ref[2]]->solution_value();
  for (auto e : comp) {
    int comp_val = x[e[0]]->solution_value() + x[e[1]]->solution_value() +
                   x[e[2]]->solution_value();
    EXPECT_EQ(val, comp_val);
  }
}

/**
 * Test branching priority on the following mip
 *    max  .5x + .86y
 *    st. 4.2x + 2y >= 33.6
 *        1.3x + 2y <= 16.2
 *          x ,  y \in N
 */
TEST(KnitroInterface, BranchingPriorityChangedForVariable) {
  UNITTEST_INIT_MIP();
  double infinity = solver.infinity();
  MPVariable* const x = solver.MakeIntVar(0, infinity, "x");
  MPVariable* const y = solver.MakeIntVar(0, infinity, "y");
  MPConstraint* const c1 = solver.MakeRowConstraint(33.6, infinity, "c1");
  c1->SetCoefficient(x, 4.2);
  c1->SetCoefficient(y, 2);
  MPConstraint* const c2 = solver.MakeRowConstraint(-infinity, 16.2, "c2");
  c2->SetCoefficient(x, 1.3);
  c2->SetCoefficient(y, 2);
  MPObjective* const obj = solver.MutableObjective();
  obj->SetCoefficient(x, .5);
  obj->SetCoefficient(y, .86);
  obj->SetMaximization();

  solver.SetSolverSpecificParametersAsString("KN_PARAM_ACT_LPALG 1 KN_PARAM_MIP_LPALG 3 ");

  // Prioritize branching on x
  x->SetBranchingPriority(10);
  y->SetBranchingPriority(1);
  solver.Solve();
  double sol1 = obj->Value();
  int nodes1 = solver.nodes();

  // Prioritize branching on y
  x->SetBranchingPriority(1);
  y->SetBranchingPriority(10);
  solver.Solve();
  double sol2 = obj->Value();
  int nodes2 = solver.nodes();

  EXPECT_EQ(sol1, sol2);
  EXPECT_NE(nodes1, nodes2);
}

// ----- Change Var/Con/obj

/**
 * Solves the initial problem
 *    max   x
 *    st.   x +  y >= 2
 *        -2x +  y <= 4
 *          x +  y <= 10
 *          x -  y <= 8
 *          x ,  y >= 0
 * Then applies changes in the problem
 */
TEST(KnitroInterface, ChangePostsolve) {
  UNITTEST_INIT_LP();
  const double infinity = solver.solver_infinity();

  MPVariable* const x = solver.MakeNumVar(0.0, infinity, "x");
  MPVariable* const y = solver.MakeNumVar(0.0, infinity, "y");

  MPConstraint* const c1 = solver.MakeRowConstraint(2, infinity, "c1");
  c1->SetCoefficient(x, 1);
  c1->SetCoefficient(y, 1);
  MPConstraint* const c2 = solver.MakeRowConstraint(-infinity, 4, "c2");
  c2->SetCoefficient(x, -2);
  c2->SetCoefficient(y, 1);
  MPConstraint* const c3 = solver.MakeRowConstraint(-infinity, 10, "c3");
  c3->SetCoefficient(x, 1);
  c3->SetCoefficient(y, 1);
  MPConstraint* const c4 = solver.MakeRowConstraint(-infinity, 8, "c4");
  c4->SetCoefficient(x, 1);
  c4->SetCoefficient(y, -1);

  MPObjective* const obj = solver.MutableObjective();
  obj->SetCoefficient(x, 1);
  obj->SetMaximization();

  solver.Solve();
  EXPECT_NEAR(obj->Value(), 9, ERROR_RATE);

  // change the objective
  obj->SetCoefficient(x, 0);
  obj->SetCoefficient(y, 1);

  solver.Solve();
  EXPECT_NEAR(obj->Value(), 8, ERROR_RATE);

  // change the boundaries of y
  y->SetBounds(2, 4);
  solver.Solve();
  EXPECT_NEAR(obj->Value(), 4, ERROR_RATE);

  // change the boundaries of y
  //        the objective
  //        the boundaries of c4
  y->SetBounds(0, infinity);
  obj->SetCoefficient(x, 1);
  obj->SetCoefficient(y, 0);
  c4->SetBounds(2, 6);
  solver.Solve();
  EXPECT_NEAR(obj->Value(), 8, ERROR_RATE);
}

/**
 * Solves the initial problem
 *    max   x
 *    st.   x +  y >= 2
 *        -2x +  y <= 4
 *          x +  y <= 10
 *          x -  y <= 8
 *          x ,  y >= 0
 * Then applies changes in the problem
 * with incrementality option off
 */
TEST(KnitroInterface, ChangePostsolve2) {
  UNITTEST_INIT_LP();
  const double infinity = solver.solver_infinity();

  MPVariable* const x = solver.MakeNumVar(0.0, infinity, "x");
  MPVariable* const y = solver.MakeNumVar(0.0, infinity, "y");

  MPConstraint* const c1 = solver.MakeRowConstraint(2, infinity, "c1");
  c1->SetCoefficient(x, 1);
  c1->SetCoefficient(y, 1);
  MPConstraint* const c2 = solver.MakeRowConstraint(-infinity, 4, "c2");
  c2->SetCoefficient(x, -2);
  c2->SetCoefficient(y, 1);
  MPConstraint* const c3 = solver.MakeRowConstraint(-infinity, 10, "c3");
  c3->SetCoefficient(x, 1);
  c3->SetCoefficient(y, 1);
  MPConstraint* const c4 = solver.MakeRowConstraint(-infinity, 8, "c4");
  c4->SetCoefficient(x, 1);
  c4->SetCoefficient(y, -1);

  MPObjective* const obj = solver.MutableObjective();
  obj->SetCoefficient(x, 1);
  obj->SetMaximization();

  solver.Solve();
  EXPECT_NEAR(obj->Value(), 9, ERROR_RATE);

  y->SetBounds(0, infinity);
  obj->SetCoefficient(x, 1);
  obj->SetCoefficient(y, 0);
  c4->SetBounds(2, 6);

  // turn incrementality off
  MPSolverParameters params;
  params.SetIntegerParam(MPSolverParameters::INCREMENTALITY,
                         MPSolverParameters::INCREMENTALITY_OFF);
  solver.Solve(params);
  EXPECT_NEAR(obj->Value(), 8, ERROR_RATE);
}

/**
 * Solves the initial problem
 *    max   x - y
 *    st. .5x + y <= 3
 *          x + y <= 3
 * Then removes a constraint and solves the new problem
 */
TEST(KnitroInterface, ClearConstraint2) {
  UNITTEST_INIT_LP();
  double infinity = solver.infinity();
  MPVariable* const x = solver.MakeNumVar(0, infinity, "x");
  MPVariable* const y = solver.MakeNumVar(0, infinity, "y");
  MPConstraint* const c1 = solver.MakeRowConstraint(-infinity, 3, "c1");
  c1->SetCoefficient(x, .5);
  c1->SetCoefficient(y, 1);
  MPConstraint* const c2 = solver.MakeRowConstraint(-infinity, 3, "c2");
  c2->SetCoefficient(x, 1);
  c2->SetCoefficient(y, 1);
  MPObjective* const obj = solver.MutableObjective();
  obj->SetCoefficient(x, 1);
  obj->SetCoefficient(y, -1);
  obj->SetMaximization();

  solver.Solve();
  EXPECT_NEAR(3, obj->Value(), ERROR_RATE);
  EXPECT_NEAR(3, x->solution_value(), ERROR_RATE);
  EXPECT_NEAR(0, y->solution_value(), ERROR_RATE);

  c2->Clear();
  solver.Solve();
  EXPECT_NEAR(6, obj->Value(), ERROR_RATE);
  EXPECT_NEAR(6, x->solution_value(), ERROR_RATE);
  EXPECT_NEAR(0, y->solution_value(), ERROR_RATE);
}

/**
 * Solves the initial problem
 *    max   x - y
 *    st. .5x + y <= 3
 *          x + y <= 3
 * Then changes the objective and solves the new problem
 */
TEST(KnitroInterface, ClearObjective2) {
  UNITTEST_INIT_LP();
  double infinity = solver.infinity();
  MPVariable* const x = solver.MakeNumVar(0, infinity, "x");
  MPVariable* const y = solver.MakeNumVar(0, infinity, "y");
  MPConstraint* const c1 = solver.MakeRowConstraint(-infinity, 3, "c1");
  c1->SetCoefficient(x, .5);
  c1->SetCoefficient(y, 1);
  MPConstraint* const c2 = solver.MakeRowConstraint(-infinity, 3, "c2");
  c2->SetCoefficient(x, 1);
  c2->SetCoefficient(y, 1);
  MPObjective* const obj = solver.MutableObjective();
  obj->SetCoefficient(x, 1);
  obj->SetCoefficient(y, -1);
  obj->SetMaximization();

  solver.Solve();
  EXPECT_NEAR(3, obj->Value(), ERROR_RATE);
  EXPECT_NEAR(3, x->solution_value(), ERROR_RATE);
  EXPECT_NEAR(0, y->solution_value(), ERROR_RATE);

  obj->Clear();
  obj->SetCoefficient(x, 1);
  obj->SetCoefficient(y, 1);
  solver.Solve();
  EXPECT_NEAR(0, obj->Value(), ERROR_RATE);
  EXPECT_NEAR(0, x->solution_value(), ERROR_RATE);
  EXPECT_NEAR(0, y->solution_value(), ERROR_RATE);
}

/**
 * Solves the initial problem
 *    max   x
 *    st.   x + y >= 2.5
 *          x + y >= -2.5
 *          x - y <= 2.5
 *          x - y >= -2.5
 *          x , y \in R
 * Then changes x into integer type and solves the new problem
 * Finally changes x back into continuous var and solves
 */
TEST(KnitroInterface, ChangeVarIntoInteger) {
  UNITTEST_INIT_MIP();
  double infinity = solver.solver_infinity();
  MPVariable* const x = solver.MakeNumVar(-infinity, infinity, "x");
  MPVariable* const y = solver.MakeNumVar(-infinity, infinity, "y");
  MPConstraint* const c1 = solver.MakeRowConstraint(-infinity, 2.5, "c1");
  c1->SetCoefficient(x, 1);
  c1->SetCoefficient(y, 1);
  MPConstraint* const c2 = solver.MakeRowConstraint(-2.5, infinity, "c2");
  c2->SetCoefficient(x, 1);
  c2->SetCoefficient(y, 1);
  MPConstraint* const c3 = solver.MakeRowConstraint(-infinity, 2.5, "c3");
  c3->SetCoefficient(x, 1);
  c3->SetCoefficient(y, -1);
  MPConstraint* const c4 = solver.MakeRowConstraint(-2.5, infinity, "c4");
  c4->SetCoefficient(x, 1);
  c4->SetCoefficient(y, -1);

  MPObjective* const obj = solver.MutableObjective();
  obj->SetCoefficient(x, 1);
  obj->SetMaximization();

  solver.Solve();
  EXPECT_NEAR(obj->Value(), 2.5, ERROR_RATE);

  // Change x into integer
  x->SetInteger(true);
  solver.Solve();
  EXPECT_NEAR(obj->Value(), 2, ERROR_RATE);

  // Change x back into continuous
  x->SetInteger(false);
  solver.Solve();
  EXPECT_NEAR(obj->Value(), 2.5, ERROR_RATE);
}

/**
 * Solves the initial problem
 *    max x + y
 *    st. 0 <= x,y <= 1
 * Then changes the problem into
 *    max x + y + z
 *    st. 0 <= z <= 1 (c)
 *        0 <= x,y <= 1
 *        z >= 0
 */
TEST(KnitroInterface, AddVarAndConstraint) {
  UNITTEST_INIT_MIP();
  double infinity = solver.solver_infinity();
  MPVariable* const x = solver.MakeNumVar(0, 1, "x");
  MPVariable* const y = solver.MakeNumVar(0, 1, "y");

  MPObjective* const obj = solver.MutableObjective();
  obj->SetCoefficient(x, 1);
  obj->SetCoefficient(y, 1);
  obj->SetMaximization();

  solver.Solve();
  EXPECT_NEAR(obj->Value(), 2, ERROR_RATE);

  MPVariable* const z = solver.MakeNumVar(0, infinity, "z");
  MPConstraint* const c = solver.MakeRowConstraint(0, 1, "c");
  c->SetCoefficient(z, 1);
  obj->SetCoefficient(z, 1);

  solver.Solve();
  EXPECT_NEAR(obj->Value(), 3, ERROR_RATE);
}

/**
 * Solves the initial problem
 *    max x
 *    st. x <= 7
 *        x <= 4
 *        x >= 0
 *
 * Then add a new variable to the problem
 *    max x +  y
 *    st. x + 2y <= 7
 *        x -  y <= 4
 *        x >= 0
 */
TEST(KnitroInterface, AddVarToExistingConstraint) {
  UNITTEST_INIT_LP();
  double infinity = solver.infinity();
  MPVariable* const x = solver.MakeNumVar(0, infinity, "x");

  MPObjective* const obj = solver.MutableObjective();
  obj->SetCoefficient(x, 1);
  obj->SetMaximization();

  MPConstraint* const c1 = solver.MakeRowConstraint(-infinity, 7, "c1");
  c1->SetCoefficient(x, 1);
  MPConstraint* const c2 = solver.MakeRowConstraint(-infinity, 4, "c2");
  c2->SetCoefficient(x, 1);

  solver.Solve();
  EXPECT_NEAR(obj->Value(), 4, ERROR_RATE);

  MPVariable* const y = solver.MakeNumVar(0, infinity, "y");
  c1->SetCoefficient(y, 2);
  c2->SetCoefficient(y, -1);
  obj->SetCoefficient(y, 1);

  solver.Solve();
  EXPECT_NEAR(obj->Value(), 6, ERROR_RATE);
  EXPECT_NEAR(x->solution_value(), 5, ERROR_RATE);
  EXPECT_NEAR(y->solution_value(), 1, ERROR_RATE);
}

/**
 * Since GLOP do not implement Write method
 *
 */
void write_readible_problem_model(MPSolver& solver, const char* file_name) {
  FILE* file;
  file = fopen(file_name, "w");

  // writing variables
  int nb_var = solver.NumVariables();
  fprintf(file, "nb variables : %i\n", nb_var);
  for (int i = 0; i < nb_var; i++) {
    fprintf(file, "%4.0f %4.0f\n", solver.variable(i)->ub(),
            solver.variable(i)->lb());
  }
  fprintf(file, "\n");

  // writing A matrix
  int nb_con = solver.NumConstraints();
  fprintf(file, "A : %i x %i\n", nb_con, nb_var);
  for (int j = 0; j < nb_con; j++) {
    MPConstraint* ct = solver.constraint(j);
    for (int i = 0; i < nb_var; i++) {
      fprintf(file, "%3.0f ", ct->GetCoefficient(solver.variable(i)));
    }
    fprintf(file, "\n");
  }
  fprintf(file, "\n");

  // writing cT vector
  MPObjective* const obj = solver.MutableObjective();
  for (int i = 0; i < nb_var; i++)
    fprintf(file, "%3.0f ", obj->GetCoefficient(solver.variable(i)));

  fclose(file);
}

/**
 * Solve a random generated LP
 * max cT.x
 * st. Ax <= b
 *     u <= x <= v
 * With
 *  x \in R^n
 *  Matrix A a randomly generated invertible lower triangular matrix
 *  u and v the lower and upper bound of x respectively
 */
TEST(KnitroInterface, RandomLP) {
  srand(time(NULL));

  // Create a LP problem with Knitro solver
  UNITTEST_INIT_LP();
  double infinity = solver.infinity();
  const int nb_var = 100;
  std::vector<MPVariable*> vars;
  for (int i = 0; i < nb_var; i++)
    vars.push_back(solver.MakeNumVar(-rand() % 1000, rand() % 1000,
                                     "x_" + std::to_string(i)));
  std::vector<MPConstraint*> cons;
  for (int j = 0; j < nb_var; j++) {
    cons.push_back(solver.MakeRowConstraint(-infinity, rand() % 2001 - 1000,
                                            "c_" + std::to_string(j)));
    for (int i = 0; i <= j; i++) {
      cons.back()->SetCoefficient(
          vars[i], (i == j) ? rand() % 100 + 1 : rand() % 199 - 99);
    }
  }
  MPObjective* const objective = solver.MutableObjective();
  for (int i = 0; i < nb_var; i++) {
    objective->SetCoefficient(vars[i], rand() % 199 - 99);
  }
  objective->SetMaximization();
  time_t start_time;
  time(&start_time);
  MPSolver::ResultStatus kc_status = solver.Solve();
  printf("KNITRO solving time = %ld\n", time(NULL) - start_time);
  write_readible_problem_model(solver, "lp_problem_knitro.mps");

  // Create the same problem with GLOP solver
  MPSolver solverbis("GLOP_LP", MPSolver::GLOP_LINEAR_PROGRAMMING);
  for (int i = 0; i < nb_var; i++)
    solverbis.MakeNumVar(vars[i]->lb(), vars[i]->ub(), vars[i]->name());
  for (int j = 0; j < nb_var; j++) {
    MPConstraint* ct = solverbis.MakeRowConstraint(cons[j]->lb(), cons[j]->ub(),
                                                   cons[j]->name());
    for (int i = 0; i <= j; i++) {
      ct->SetCoefficient(solverbis.variable(i),
                         cons[j]->GetCoefficient(vars[i]));
    }
  }
  MPObjective* const objectivebis = solverbis.MutableObjective();
  for (int i = 0; i < nb_var; i++) {
    objectivebis->SetCoefficient(solverbis.variable(i),
                                 objective->GetCoefficient(vars[i]));
  }
  objectivebis->SetMaximization();
  time(&start_time);
  MPSolver::ResultStatus glop_status = solverbis.Solve();
  printf("GLOP solving time = %ld\n", time(NULL) - start_time);
  write_readible_problem_model(solverbis, "lp_problem_glop.mps");

  EXPECT_EQ(kc_status, glop_status);
  if (glop_status == MPSolver::ResultStatus::OPTIMAL) {
    EXPECT_NEAR(objective->Value(), objectivebis->Value(), 1e-3);
  }
}

// /**
//  * Solve a random generated MIP with SCIP and Knitro solver
//  * max cT.x
//  * st. Ax <= b
//  *     u <= x <= v
//  * With
//  *  x \in Z^n
//  *  Matrix A a randomly generated invertible lower triangular matrix
//  *  u and v the lower and upper bound of x respectively
//  */
// TEST(KnitroInterface, RandomMIP) {
//   srand(time(NULL));

//   // Create a LP problem with Knitro solver
//   UNITTEST_INIT_MIP();

//   double infinity = solver.infinity();
//   const int nb_var = 30;
//   std::vector<MPVariable*> vars;
//   for (int i = 0; i < nb_var; i++)
//     vars.push_back(solver.MakeIntVar(-rand() % 1000, rand() % 1000,
//                                      "x_" + std::to_string(i)));
//   std::vector<MPConstraint*> cons;
//   for (int j = 0; j < nb_var; j++) {
//     cons.push_back(solver.MakeRowConstraint(-infinity, rand() % 2001 - 1000,
//                                             "c_" + std::to_string(j)));
//     for (int i = 0; i <= j; i++) {
//       cons.back()->SetCoefficient(
//           vars[i], (i == j) ? rand() % 100 + 1 : rand() % 199 - 99);
//     }
//   }
//   MPObjective* const objective = solver.MutableObjective();
//   for (int i = 0; i < nb_var; i++) {
//     objective->SetCoefficient(vars[i], rand() % 199 - 99);
//   }
//   objective->SetMaximization();
//   time_t start_time;
//   time(&start_time);
//   MPSolver::ResultStatus kc_status = solver.Solve();
//   printf("KNITRO solving time = %ld\n", time(NULL) - start_time);
//   write_readible_problem_model(solver, "mip_problem_knitro.mps");

//   // Create the same problem with SCIP solver
//   MPSolver solverbis("SCIP_MIP", MPSolver::SCIP_MIXED_INTEGER_PROGRAMMING);
//   for (int i = 0; i < nb_var; i++)
//     solverbis.MakeIntVar(vars[i]->lb(), vars[i]->ub(), vars[i]->name());
//   for (int j = 0; j < nb_var; j++) {
//     MPConstraint* ct = solverbis.MakeRowConstraint(cons[j]->lb(), cons[j]->ub(),
//                                                    cons[j]->name());
//     for (int i = 0; i <= j; i++) {
//       ct->SetCoefficient(solverbis.variable(i),
//                          cons[j]->GetCoefficient(vars[i]));
//     }
//   }
//   MPObjective* const objectivebis = solverbis.MutableObjective();
//   for (int i = 0; i < nb_var; i++) {
//     objectivebis->SetCoefficient(solverbis.variable(i),
//                                  objective->GetCoefficient(vars[i]));
//   }
//   objectivebis->SetMaximization();
//   time(&start_time);
//   MPSolver::ResultStatus scip_status = solverbis.Solve();
//   printf("SCIP solving time = %ld\n", time(NULL) - start_time);
//   write_readible_problem_model(solverbis, "mip_problem_scip.mps");

//   EXPECT_EQ(kc_status, scip_status);
//   if (scip_status == MPSolver::ResultStatus::OPTIMAL) {
//     EXPECT_NEAR(objective->Value(), objectivebis->Value(), 1e-3);
//   }
// }

/*-------------------- Callback --------------------*/

/**
 * Test for lazy constraint extracted from Knitro's distribution
 * C code examples
 */

/**
 * User structure for the lazy constraints callback.
 */
typedef struct LazyConstraintsCallbackUserParams {
  /** Number of vertices. */
  int number_of_vertices;

  /** Pointer to the distance matrix. */
  int*** distances;

  /** Pointer to the variable indices. */
  int*** variable_indices;

} LazyConstraintsCallbackUserParams;

class KNMPCallback : public MPCallback {
 private:
  LazyConstraintsCallbackUserParams* userparams_;
  KN_context* kc_ptr_;
  MPSolver* mpSolver_;

 public:
  KNMPCallback(LazyConstraintsCallbackUserParams* userParams, KN_context* kc,
               MPSolver* solver)
      : MPCallback(false, true),
        userparams_(userParams),
        kc_ptr_(kc),
        mpSolver_(solver){};

  ~KNMPCallback() override{};

  void RunCallback(MPCallbackContext* callback_context) override {
    // Return code of Knitro API.
    int knitro_return_code = 0;

    double infinity = mpSolver_->infinity();

    // Avoid
    // 'error: 'for' loop initial declarations are only allowed in C99 mode'
    int vertex_id_1 = -1;
    int vertex_id_2 = -1;
    int pos = -1;

    // We only add cuts for integral solutions.
    // Retrieve the integer tolerance value.
    double integer_tol = 0.0;
    knitro_return_code =
        KN_get_double_param(kc_ptr_, KN_PARAM_MIP_INTEGERTOL, &integer_tol);
    if (knitro_return_code) return;

    // Stop if the solution is not integral.
    // printf("  Check if solution is integral.\n");
    for (vertex_id_1 = 0; vertex_id_1 < userparams_->number_of_vertices;
         ++vertex_id_1) {
      for (vertex_id_2 = 0; vertex_id_2 < vertex_id_1; ++vertex_id_2) {
        int variable_id =
            (*userparams_->variable_indices)[vertex_id_1][vertex_id_2];
        double value =
            callback_context->VariableValue(mpSolver_->variable(variable_id));
        if (fabs(value - round(value)) > integer_tol) {
          // printf("  The solution is not integral.\n\n");
          return;
        }
      }
    }
    // printf("  The solution is integral.\n");

    // Get objective value.
    double objective = -1;
    knitro_return_code = KN_get_obj_value(kc_ptr_, &objective);
    if (knitro_return_code) return;

    // printf(
    //         "  Lazy constraints called on an integral solution of value
    //         %e.\n", objective);

    // For each vertex, retrieve its two neighbors.
    int(*neighbors)[2];
    neighbors =
        (int(*)[2])malloc(userparams_->number_of_vertices * sizeof(int[2]));
    for (vertex_id_1 = 0; vertex_id_1 < userparams_->number_of_vertices;
         ++vertex_id_1) {
      neighbors[vertex_id_1][0] = -1;
      neighbors[vertex_id_1][1] = -1;
    }
    for (vertex_id_1 = 0; vertex_id_1 < userparams_->number_of_vertices;
         ++vertex_id_1) {
      for (vertex_id_2 = 0; vertex_id_2 < vertex_id_1; ++vertex_id_2) {
        int variable_id =
            (*userparams_->variable_indices)[vertex_id_1][vertex_id_2];
        double value =
            callback_context->VariableValue(mpSolver_->variable(variable_id));
        if (value > 0.5) {
          if (neighbors[vertex_id_1][0] == -1) {
            neighbors[vertex_id_1][0] = vertex_id_2;
          } else {
            neighbors[vertex_id_1][1] = vertex_id_2;
          }
          if (neighbors[vertex_id_2][0] == -1) {
            neighbors[vertex_id_2][0] = vertex_id_1;
          } else {
            neighbors[vertex_id_2][1] = vertex_id_1;
          }
        }
      }
    }

    // For each vertex, the sub-tour it belongs to.
    int* vertex_sub_tour;
    vertex_sub_tour =
        (int*)malloc(userparams_->number_of_vertices * sizeof(int));
    for (vertex_id_1 = 0; vertex_id_1 < userparams_->number_of_vertices;
         ++vertex_id_1) {
      vertex_sub_tour[vertex_id_1] = -1;
    }

    // The current sub-tour.
    int* current_sub_tour;
    current_sub_tour =
        (int*)malloc(userparams_->number_of_vertices * sizeof(int));
    int current_sub_tour_size = 0;

    // Find the sub-tours.
    int number_of_sub_tours = 0;
    for (;;) {
      // Find the first vertex which sub-tour has not been found.
      int vertex_id = 0;
      while (vertex_id < userparams_->number_of_vertices &&
             vertex_sub_tour[vertex_id] != -1) {
        vertex_id++;
      }
      if (vertex_id == userparams_->number_of_vertices) break;

      // Loop through the tour starting at vertex 'vertex_id'.
      current_sub_tour_size = 0;
      while (vertex_sub_tour[vertex_id] == -1) {
        vertex_sub_tour[vertex_id] = number_of_sub_tours;
        current_sub_tour[current_sub_tour_size] = vertex_id;
        if (current_sub_tour_size == 0 ||
            neighbors[vertex_id][0] !=
                current_sub_tour[current_sub_tour_size - 1]) {
          vertex_id = neighbors[vertex_id][0];
        } else {
          vertex_id = neighbors[vertex_id][1];
        }
        current_sub_tour_size++;
      }

      // Add the sub-tour elimination constraint.
      if (current_sub_tour_size < userparams_->number_of_vertices) {
        number_of_sub_tours++;

        LinearExpr ct;
        for (pos = 0; pos < current_sub_tour_size; ++pos) {
          int vertex_id_1 = current_sub_tour[pos];
          int vertex_id_2 = current_sub_tour[(pos + 1) % current_sub_tour_size];
          int variable_id =
              (vertex_id_1 > vertex_id_2)
                  ? (*userparams_->variable_indices)[vertex_id_1][vertex_id_2]
                  : (*userparams_->variable_indices)[vertex_id_2][vertex_id_1];
          ct += LinearExpr(mpSolver_->variable(variable_id));
        }

        callback_context->AddLazyConstraint(
            LinearRange(-infinity, ct, current_sub_tour_size - 1));
      }
    }

    // if (number_of_sub_tours == 0) {
    //     printf("  No sub-tour found, the solution is feasible.\n");
    // } else {
    //     printf("  %i sub-tour(s) found.\n", number_of_sub_tours);
    // }
    // printf("\n");

    // Free allocated structures.
    free(neighbors);
    free(vertex_sub_tour);
    free(current_sub_tour);

    return;
  };

  LazyConstraintsCallbackUserParams* GetUserParams() { return userparams_; }
};

TEST(KnitroInterface, LazyConstraint) {
  // Avoid
  // 'error: 'for' loop initial declarations are only allowed in C99 mode'
  int vertex_id_1 = -1;
  int vertex_id_2 = -1;

  int fscanf_return_value = -1;

  //////////
  // Data //
  //////////

  // Data of the traveling salesman problem.
  std::filesystem::path p = std::filesystem::current_path();
  std::cout << "Current path" << p << std::endl;
  FILE* file = fopen("ortools/knitro/resources/bayg29.tsp", "r");
  if (file != nullptr) {
    printf("bayg29.tsp found in Knitro resources rep\n");
  } else {
    file = fopen("or-tools/ortools/knitro/resources/bayg29.tsp", "r");
    if (file != nullptr) {
      printf("bayg29.tsp found in submodule OR-Tools Knitro resources rep\n");
    } else {
      file = fopen(absl::StrCat(getenv("OR_ROOT"), "/ortools/knitro/resources/bayg29.tsp"), "r");
      if (file != nullptr) {
        printf("bayg29.tsp found with OR_ROOT %s\n", getenv("OR_ROOT"));
      } else {
        printf("bayg29.tsp not found !\n");
        EXPECT_TRUE(false);
      }
    }
  }

  int number_of_vertices = -1;
  fscanf_return_value = fscanf(file, "%d", &number_of_vertices);
  EXPECT_EQ(fscanf_return_value, 1);

  int** distances;
  distances = (int**)malloc(number_of_vertices * sizeof(int*));
  for (vertex_id_1 = 1; vertex_id_1 < number_of_vertices; ++vertex_id_1) {
    distances[vertex_id_1] = (int*)malloc(vertex_id_1 * sizeof(int));
  }
  for (vertex_id_1 = 0; vertex_id_1 < number_of_vertices; ++vertex_id_1) {
    for (vertex_id_2 = 0; vertex_id_2 < vertex_id_1; ++vertex_id_2) {
      int distance = 0;
      fscanf_return_value = fscanf(file, "%i", &distance);
      EXPECT_EQ(fscanf_return_value, 1);
      distances[vertex_id_1][vertex_id_2] = distance;
    }
  }
  fclose(file);

  ///////////////////////////////
  // Initialize Knitro context //
  ///////////////////////////////

  // Return code of Knitro API.
  int knitro_return_code = 0;

  // Create a new Knitro context.
  UNITTEST_INIT_MIP();
  MPObjective* obj = solver.MutableObjective();

  ///////////////
  // Variables //
  ///////////////

  // Structure to retrieve variable indices from vertex indices.
  int** variable_indices;
  variable_indices = (int**)malloc(number_of_vertices * sizeof(int*));
  for (vertex_id_1 = 1; vertex_id_1 < number_of_vertices; ++vertex_id_1) {
    variable_indices[vertex_id_1] = (int*)malloc(vertex_id_1 * sizeof(int));
  }

  // Add variables.
  for (vertex_id_1 = 0; vertex_id_1 < number_of_vertices; ++vertex_id_1) {
    for (vertex_id_2 = 0; vertex_id_2 < vertex_id_1; ++vertex_id_2) {
      // Add the variable corresponding to edge (vertex_id_1, vertex_id_2).
      int variable_id = -1;
      MPVariable* var_tmp = solver.MakeBoolVar("");
      variable_id = var_tmp->index();
      variable_indices[vertex_id_1][vertex_id_2] = variable_id;
    }
  }

  ///////////////
  // Objective //
  ///////////////

  // Set the objective.
  for (vertex_id_1 = 0; vertex_id_1 < number_of_vertices; ++vertex_id_1) {
    for (vertex_id_2 = 0; vertex_id_2 < vertex_id_1; ++vertex_id_2) {
      obj->SetCoefficient(
          solver.variable(variable_indices[vertex_id_1][vertex_id_2]),
          distances[vertex_id_1][vertex_id_2]);
    }
  }

  /////////////////
  // Constraints //
  /////////////////

  // For each vertex, select exactly two incident edges.
  for (vertex_id_1 = 0; vertex_id_1 < number_of_vertices; ++vertex_id_1) {
    // Add the constraint.
    MPConstraint* ct = solver.MakeRowConstraint(2, 2);

    // Add the constraint terms.
    for (vertex_id_2 = 0; vertex_id_2 < number_of_vertices; ++vertex_id_2) {
      if (vertex_id_2 == vertex_id_1) continue;
      int variable_id = (vertex_id_1 > vertex_id_2)
                            ? variable_indices[vertex_id_1][vertex_id_2]
                            : variable_indices[vertex_id_2][vertex_id_1];
      ct->SetCoefficient(solver.variable(variable_id), 1);
    }
  }

  // Sub-tour elimination constraints added through lazy constraints.
  LazyConstraintsCallbackUserParams lazyconstraints_callback_user_params;
  lazyconstraints_callback_user_params.number_of_vertices = number_of_vertices;
  lazyconstraints_callback_user_params.distances = &distances;
  lazyconstraints_callback_user_params.variable_indices = &variable_indices;
  KNMPCallback* callback = new KNMPCallback(
      &lazyconstraints_callback_user_params,
      reinterpret_cast<KN_context*>(solver.underlying_solver()), &solver);
  solver.SetCallback(callback);

  ///////////
  // Solve //
  ///////////

  solver.Solve();

  ///////////////////////////////
  // Free allocated structures //
  ///////////////////////////////

  // Delete distances and variable_indices.
  for (vertex_id_1 = 1; vertex_id_1 < number_of_vertices; ++vertex_id_1) {
    free(distances[vertex_id_1]);
    free(variable_indices[vertex_id_1]);
  }
  free(distances);
  free(variable_indices);
  free(callback);

  double integer_tol;
  getter.Double_Param(KN_PARAM_MIP_INTEGERTOL, &integer_tol);

  EXPECT_NEAR(obj->Value(), 1610, integer_tol);
}

}  // namespace operations_research

int main(int argc, char** argv) {
  absl::SetFlag(&FLAGS_logtostderr, 1);
  testing::InitGoogleTest(&argc, argv);
  if (!operations_research::KnitroIsCorrectlyInstalled()) {
    LOG(ERROR) << "Knitro solver is not available";
    return EXIT_SUCCESS;
  } else {
    if (!RUN_ALL_TESTS()) {
      remove("knitro_interface_test_LP_model");
      remove("knitro_interface_test_empty");
      remove("lp_problem_knitro.mps");
      remove("lp_problem_glop.mps");
      remove("mip_problem_knitro.mps");
      remove("mip_problem_scip.mps");
      return EXIT_SUCCESS;
    }
    return EXIT_FAILURE;
  }
}
