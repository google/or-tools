// ADD HEADER
#include <stdio.h>
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

// TODO : mettre var glob marge erreur

#define EXPECT_STATUS(s)                              \
  do {                                                \
    int const status_ = s;                            \
    EXPECT_EQ(0, status_) << "Nonzero return status"; \
  } while (0)

inline bool file_exists(const std::string& name) {
  if (FILE *file = fopen(name.c_str(), "r")) {
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

/*-------------------- Check Env --------------------*/

TEST(Env, CheckEnv) { EXPECT_EQ(KnitroIsCorrectlyInstalled(), true); }

/*-------------------- TU --------------------*/

// ----- Empty model

/** Unit Test to solve an empty LP */ 
TEST(KnitroInterface, EmptyLP) {
  UNITTEST_INIT_LP();
  solver.Solve();
  EXPECT_EQ(solver.MutableObjective()->Value(), 0);
}

/** Unit Test to solve an empty MIP */
TEST(KnitroInterface, EmptyMIP) {
  UNITTEST_INIT_MIP();
  solver.Solve();
  EXPECT_EQ(solver.MutableObjective()->Value(), 0);
}

/** Unit Test to write an empty MIP */ 
TEST(KnitroInterface, WriteEmpty) {
  UNITTEST_INIT_MIP();
  solver.Write("knitro_interface_test_empty");
  // check the file exists
  EXPECT_TRUE(file_exists("knitro_interface_test_empty"));
}

// ----- Modeling functions

/** Unit Test of the method infinity()*/
TEST(KnitroInterface, infinity){
  UNITTEST_INIT_LP();
  EXPECT_EQ(solver.solver_infinity(), KN_INFINITY);
}

/** Unit Test of the method AddVariable()*/
TEST(KnitroInterface, AddVariable){
  UNITTEST_INIT_LP();
  MPVariable* const x = solver.MakeNumVar(0,10,"x");
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
TEST(KnitroInterface, AddRowConstraint) {
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
TEST(KnitroInterface, SetCoefficient) {
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
}

/** Unit Test of the method ClearConstraint()*/
TEST(KnitroInterface, ClearConstraint) {
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
TEST(KnitroInterface, SetObjectiveCoefficient) {
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
TEST(KnitroInterface, SetOptimizationDirection) {
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
TEST(KnitroInterface, SetObjectiveOffset) {
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
TEST(KnitroInterface, Reset){
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

  solver.Solve(); // to extract the model
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
TEST(KnitroInterface, SetScaling) {
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
TEST(KnitroInterface, SetRelativeMipGap) {
  UNITTEST_INIT_MIP();
  MPSolverParameters params;
  params.SetDoubleParam(MPSolverParameters::RELATIVE_MIP_GAP, 0.5);
  solver.Solve(params);
  double value;
  getter.Double_Param(KN_PARAM_MIP_OPTGAPREL, &value);
  EXPECT_EQ(value, .5);
  params.SetDoubleParam(MPSolverParameters::RELATIVE_MIP_GAP, 1e-6);
  solver.Solve(params);
  getter.Double_Param(KN_PARAM_MIP_OPTGAPREL, &value);
  EXPECT_EQ(value, 1e-6);
}

/** Unit Test of the method SetPresolveMode()*/
TEST(KnitroInterface, SetPresolveMode) {
  UNITTEST_INIT_MIP();
  MPSolverParameters params;
  params.SetIntegerParam(MPSolverParameters::PRESOLVE,
                         MPSolverParameters::PRESOLVE_OFF);
  solver.Solve(params);
  int value;
  getter.Int_Param(KN_PARAM_PRESOLVE, &value);
  EXPECT_EQ(value, KN_PRESOLVE_NO);
  params.SetIntegerParam(MPSolverParameters::PRESOLVE,
                         MPSolverParameters::PRESOLVE_ON);
  solver.Solve(params);
  getter.Int_Param(KN_PARAM_PRESOLVE, &value);
  EXPECT_EQ(value, KN_PRESOLVE_YES);
}

/** Unit Test of the method SetSolverSpecificParametersAsString()*/
TEST(KnitroInterface, SetSolverSpecificParametersAsString) {
  UNITTEST_INIT_MIP();
  std::ofstream param_file;
  param_file.open("knitro_interface__test_param.opt");
  param_file << "feastol   1e-08\n";
  param_file << "linsolver_scaling always";
  param_file.close();
  solver.SetSolverSpecificParametersAsString(
      "knitro_interface__test_param.opt");
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
}

/** Unit Test of the method SetPrimalTolerance()*/
TEST(KnitroInterface, SetPrimalTolerance) {
  UNITTEST_INIT_LP();
  MPSolverParameters params;
  params.SetDoubleParam(MPSolverParameters::PRIMAL_TOLERANCE, 0.5);
  solver.Solve(params);
  double value;
  getter.Double_Param(KN_PARAM_FEASTOL, &value);
  EXPECT_EQ(value, .5);
  params.SetDoubleParam(MPSolverParameters::PRIMAL_TOLERANCE, 1e-6);
  solver.Solve(params);
  getter.Double_Param(KN_PARAM_FEASTOL, &value);
  EXPECT_EQ(value, 1e-6);
}

/** Unit Test of the method SetDualTolerance()*/
TEST(KnitroInterface, SetDualTolerance) {
  UNITTEST_INIT_LP();
  MPSolverParameters params;
  params.SetDoubleParam(MPSolverParameters::DUAL_TOLERANCE, 0.5);
  solver.Solve(params);
  double value;
  getter.Double_Param(KN_PARAM_OPTTOL, &value);
  EXPECT_EQ(value, .5);
  params.SetDoubleParam(MPSolverParameters::DUAL_TOLERANCE, 1e-6);
  solver.Solve(params);
  getter.Double_Param(KN_PARAM_OPTTOL, &value);
  EXPECT_EQ(value, 1e-6);
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

/** Unit Test of the method underlying_solver()*/
TEST(KnitroInterface, underlying_solver){
  UNITTEST_INIT_LP();
  auto ptr = solver.underlying_solver();
  EXPECT_NE(ptr, nullptr);
}

// ----- Getting post-solve informations

/** Unit Test of the method nodes()*/
TEST(KnitroInterface, nodes){
  UNITTEST_INIT_MIP();
  EXPECT_EQ(solver.nodes(), MPSolverInterface::kUnknownNumberOfNodes);
  solver.Solve();
  EXPECT_NE(solver.nodes(), MPSolverInterface::kUnknownNumberOfNodes);
}

/** Unit Test of the method iterations()*/
TEST(KnitroInterface, iterations){
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
 */
TEST(KnitroInterface, WriteLoadModel) {
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
  EXPECT_NEAR(kc_x[0], 6, 1e-6);
  EXPECT_NEAR(kc_x[1], 2, 1e-6);
  EXPECT_NEAR(objSol, 10, 1e-6);

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

  EXPECT_NEAR(obj->Value(), 9.4, 1e-6);
  EXPECT_NEAR(x->solution_value(), 1.8, 1e-6);
  EXPECT_NEAR(y->solution_value(), 2.8, 1e-6);
  EXPECT_NEAR(x->reduced_cost(), 0, 1e-6);
  EXPECT_NEAR(y->reduced_cost(), 0, 1e-6);
  EXPECT_NEAR(c1->dual_value(), 0.2, 1e-6);
  EXPECT_NEAR(c2->dual_value(), 0, 1e-6);
  EXPECT_NEAR(c3->dual_value(), 0.6, 1e-6);
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
  EXPECT_NEAR(objective->Value(), 40, 1e-7);
  EXPECT_NEAR(x->solution_value(), 10, 1e-7);
  EXPECT_NEAR(y->solution_value(), 0, 1e-7);
  EXPECT_NEAR(z->solution_value(), 6, 1e-7);

  // Just Check that the methods return something

  EXPECT_NE(solver.nodes(), MPSolverInterface::kUnknownNumberOfNodes);
  EXPECT_NE(solver.iterations(), MPSolverInterface::kUnknownNumberOfIterations);
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
  EXPECT_NEAR(11, obj->Value(), 1e-6);
  EXPECT_NEAR(5, x->solution_value(), 1e-6);
  EXPECT_NEAR(3, y->solution_value(), 1e-6);

  // change boundaries to infinity
  x->SetBounds(-infinity, infinity);
  c2->SetBounds(-2, infinity);
  solver.Solve();
  EXPECT_NEAR(11, obj->Value(), 1e-6);
  EXPECT_NEAR(5, x->solution_value(), 1e-6);
  EXPECT_NEAR(3, y->solution_value(), 1e-6);
}

/**
 * Solves a LP without counstraints
 *    max x + y + z
 *    st. x,  y,  z >= 0
 *        x,  y,  z <= 1
 */
TEST(KnitroInterface, JustVar) {
  UNITTEST_INIT_LP();
  std::vector<MPVariable*> x;
  solver.MakeNumVarArray(3, 0, 1, "x", &x);
  MPObjective* const obj = solver.MutableObjective();
  obj->SetCoefficient(x[0], 1);
  obj->SetCoefficient(x[1], 1);
  obj->SetCoefficient(x[2], 1);
  obj->SetMaximization();

  solver.Solve();
  EXPECT_NEAR(obj->Value(), 3, 1e-6);
}

/**
 * MIP without objective
 * It solves the [3x3 magic square problem](https://en.wikipedia.org/wiki/Magic_square)
 * by finding feasible solution
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

// Change Var/Con/obj

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
  EXPECT_NEAR(obj->Value(), 9, 1e-7);

  // change the objective
  obj->SetCoefficient(x, 0);
  obj->SetCoefficient(y, 1);

  solver.Solve();
  EXPECT_NEAR(obj->Value(), 8, 1e-7);

  // change the boundaries of y
  y->SetBounds(2, 4);
  solver.Solve();
  EXPECT_NEAR(obj->Value(), 4, 1e-7);

  // change the boundaries of y
  //        the objective
  //        the boundaries of c4
  y->SetBounds(0, infinity);
  obj->SetCoefficient(x, 1);
  obj->SetCoefficient(y, 0);
  c4->SetBounds(2, 6);
  solver.Solve();
  EXPECT_NEAR(obj->Value(), 8, 1e-7);
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
  EXPECT_NEAR(3, obj->Value(), 1e-6);
  EXPECT_NEAR(3, x->solution_value(), 1e-6);
  EXPECT_NEAR(0, y->solution_value(), 1e-6);

  c2->Clear();
  solver.Solve();
  EXPECT_NEAR(6, obj->Value(), 1e-6);
  EXPECT_NEAR(6, x->solution_value(), 1e-6);
  EXPECT_NEAR(0, y->solution_value(), 1e-6);
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
  EXPECT_NEAR(3, obj->Value(), 1e-6);
  EXPECT_NEAR(3, x->solution_value(), 1e-6);
  EXPECT_NEAR(0, y->solution_value(), 1e-6);

  obj->Clear();
  obj->SetCoefficient(x, 1);
  obj->SetCoefficient(y, 1);
  solver.Solve();
  EXPECT_NEAR(0, obj->Value(), 1e-6);
  EXPECT_NEAR(0, x->solution_value(), 1e-6);
  EXPECT_NEAR(0, y->solution_value(), 1e-6);
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
  EXPECT_NEAR(obj->Value(), 2.5, 1e-7);

  // Change x into integer
  x->SetInteger(true);
  solver.Solve();
  EXPECT_NEAR(obj->Value(), 2, 1e-7);

  // Change x back into continuous
  x->SetInteger(false);
  solver.Solve();
  EXPECT_NEAR(obj->Value(), 2.5, 1e-7);
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
  EXPECT_NEAR(obj->Value(), 2, 1e-7);

  MPVariable* const z = solver.MakeNumVar(0, infinity, "z");
  MPConstraint* const c = solver.MakeRowConstraint(0, 1, "c");
  c->SetCoefficient(z, 1);
  obj->SetCoefficient(z, 1);

  solver.Solve();
  EXPECT_NEAR(obj->Value(), 3, 1e-7);
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
TEST(KnitroInterface, AddVarToExistingConstraint){
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
  EXPECT_NEAR(obj->Value(), 4, 1e-7);

  MPVariable* const y = solver.MakeNumVar(0, infinity, "y");
  c1->SetCoefficient(y, 2);
  c2->SetCoefficient(y, -1);
  obj->SetCoefficient(y, 1);

  solver.Solve();
  EXPECT_NEAR(obj->Value(), 6, 1e-7);
  EXPECT_NEAR(x->solution_value(), 5, 1e-7);
  EXPECT_NEAR(y->solution_value(), 1, 1e-7);
}

}  // namespace operations_research

int main(int argc, char** argv) {
  absl::SetFlag(&FLAGS_logtostderr, 1);
  testing::InitGoogleTest(&argc, argv);
  if (!RUN_ALL_TESTS()) {
    remove("knitro_interface_test_LP_model");
    remove("knitro_interface__test_param.opt");
    remove("knitro_interface_test_empty");
    return EXIT_SUCCESS;
  }
  return EXIT_FAILURE;
}
