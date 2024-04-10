// Copyright 2010-2024 Google LLC
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

#define EXPECT_STATUS(s)                              \
  do {                                                \
    int const status_ = s;                            \
    EXPECT_EQ(0, status_) << "Nonzero return status"; \
  } while (0)

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

  void Con_tot_nnz(int64_t* nnz) {
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

TEST(KnitroInterface, EmptyLP) {
  UNITTEST_INIT_LP();
  solver.Solve();
  EXPECT_EQ(solver.MutableObjective()->Value(), 0);
}

TEST(KnitroInterface, EmptyMIP) {
  UNITTEST_INIT_MIP();
  solver.Solve();
  EXPECT_EQ(solver.MutableObjective()->Value(), 0);
}

TEST(KnitroInterface, WriteEmpty) {
  UNITTEST_INIT_MIP();
  solver.Write("knitro_interface_test_empty");
}

// ----- Modeling functions

TEST(KnitroInterface, AddVariable) {
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

TEST(KnitroInterface, ClearConstraint) {
  UNITTEST_INIT_LP();
  MPVariable* const x = solver.MakeNumVar(0, 10, "x");
  MPConstraint* const ct = solver.MakeRowConstraint(0, 10, "ct");
  ct->SetCoefficient(x, 2);
  int64_t nnz;
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

TEST(KnitroInterface, ClearObjective) {
  UNITTEST_INIT_LP();
  MPVariable* const x = solver.MakeNumVar(0, 1, "x");
  MPObjective* const obj = solver.MutableObjective();
  obj->SetCoefficient(x, 1);
  int nnz;
  getter.Obj_nb_coef(&nnz);
  // The objective coefficeint has not been extracted yet
  EXPECT_EQ(nnz, 0);
  solver.Solve();
  getter.Obj_nb_coef(&nnz);
  EXPECT_EQ(nnz, 1);
  obj->Clear();
  getter.Obj_nb_coef(&nnz);
  EXPECT_EQ(nnz, 0);
}

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

TEST(KnitroInterface, SetNumThreads) {
  UNITTEST_INIT_MIP();
  auto status = solver.SetNumThreads(4);
  solver.Solve();
  int value;
  getter.Int_Param(KN_PARAM_NUMTHREADS, &value);
  EXPECT_EQ(value, 4);
}

// ----- Solve

TEST(KnitroInterface, underlying_solver){
  UNITTEST_INIT_LP();
  auto ptr = solver.underlying_solver();
  EXPECT_NE(ptr, nullptr);
}

// ----- Getting post-solve informations

TEST(KitroInterface, nodes){
  UNITTEST_INIT_MIP();
  EXPECT_EQ(solver.nodes(), MPSolverInterface::kUnknownNumberOfNodes);
  solver.Solve();
  EXPECT_NE(solver.nodes(), MPSolverInterface::kUnknownNumberOfNodes);
}

TEST(KitroInterface, iterations){
  UNITTEST_INIT_MIP();
  EXPECT_EQ(solver.iterations(), MPSolverInterface::kUnknownNumberOfIterations);
  solver.Solve();
  EXPECT_NE(solver.iterations(), MPSolverInterface::kUnknownNumberOfIterations);
}

/*-------------------- TF --------------------*/

TEST(KnitroInterface, SetAndWriteModel) {
  // max  x + 2y
  // st. 3x - 4y >= 10
  //     2x + 3y <= 18
  //      x,   y \in R+

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
}

TEST(KnitroInterface, CheckWrittenModel) {
  // Read model using Knitro and solve it
  // max  x + 2y
  // st. 3x - 4y >= 10
  //     2x + 3y <= 18
  //      x,   y \in R+

  KN_context* kc;
  KN_new(&kc);
  KN_load_mps_file(kc, "knitro_interface_test_LP_model");
  KN_set_int_param(kc, KN_PARAM_OUTLEV, KN_OUTLEV_NONE);
  // Check variables
  double lb[2], ub[2];
  KN_get_var_lobnds_all(kc, lb);
  KN_get_var_upbnds_all(kc, ub);
  EXPECT_EQ(lb[0], 0);
  EXPECT_EQ(lb[1], 0);
  EXPECT_EQ(ub[0], KN_INFINITY);
  EXPECT_EQ(ub[1], KN_INFINITY);
  char* names[2];
  names[0] = new char[20];
  names[1] = new char[20];
  KN_get_var_names_all(kc, 20, names);
  EXPECT_STREQ(names[0], "x");
  EXPECT_STREQ(names[1], "y");
  // Check constraints
  KN_get_con_lobnds_all(kc, lb);
  KN_get_con_upbnds_all(kc, ub);
  EXPECT_EQ(lb[0], 10);
  EXPECT_EQ(lb[1], -KN_INFINITY);
  EXPECT_EQ(ub[0], KN_INFINITY);
  EXPECT_EQ(ub[1], 18);
  KN_get_con_names_all(kc, 20, names);
  EXPECT_STREQ(names[0], "c1");
  EXPECT_STREQ(names[1], "c2");
  // Check everything else by solving the lp
  double objSol;
  int nStatus = KN_solve(kc);
  double x[2];
  KN_get_solution(kc, &nStatus, &objSol, x, NULL);
  EXPECT_NEAR(x[0], 6, 1e-6);
  EXPECT_NEAR(x[1], 2, 1e-6);
  EXPECT_NEAR(objSol, 10, 1e-6);

  delete[] names[0];
  delete[] names[1];
  KN_free(&kc);
}

TEST(KnitroInterface, SolveLP) {
  // max   x + 2y + 2
  // st.  -x +  y <= 1
  //      3x + 2y <= 12
  //      2x + 3y <= 12
  //       x ,  y \in R+

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

TEST(KnitroInterface, SolveMIP) {
  // max  x -  y + 5z
  // st.  x + 2y -  z <= 19.5
  //      x +  y +  z >= 3.14
  //      x           <= 10
  //           y +  z <= 6
  //      x,   y,   z \in R+

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

TEST(KnitroInterface, SupportInfinity) {
  // max  x + 2y
  // st.  x + 4y >= -8
  //      x + 4y <= 17
  //     -x +  y >= -2
  //     -x +  y <=  3
  //      x,   y >= 0
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

  x->SetBounds(-infinity, infinity);
  c2->SetBounds(-2, infinity);
  solver.Solve();
  EXPECT_NEAR(11, obj->Value(), 1e-6);
  EXPECT_NEAR(5, x->solution_value(), 1e-6);
  EXPECT_NEAR(3, y->solution_value(), 1e-6);
}

TEST(KnitroInterface, JustVar) {
  // max x + y + z
  // st. x,  y,  z >= 0
  //     x,  y,  z <= 1
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

TEST(KnitroInterface, ChangePostsolve) {
  // max   x
  // st.   x +  y >= 2
  //     -2x +  y <= 4
  //       x +  y <= 10
  //       x -  y <= 8
  //       x ,  y >= 0

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

  obj->SetCoefficient(x, 0);
  obj->SetCoefficient(y, 1);

  solver.Solve();
  EXPECT_NEAR(obj->Value(), 8, 1e-7);

  y->SetBounds(2, 4);
  solver.Solve();
  EXPECT_NEAR(obj->Value(), 4, 1e-7);

  y->SetBounds(0, infinity);
  obj->SetCoefficient(x, 1);
  obj->SetCoefficient(y, 0);
  c4->SetBounds(2, 6);
  solver.Solve();
  EXPECT_NEAR(obj->Value(), 8, 1e-7);
}

TEST(KnitroInterface, ClearConstraint2) {
  // max   x - y
  // st. .5x + y <= 3
  //       x + y <= 3
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

TEST(KnitroInterface, ClearObjective2) {
  // max   x - y
  // st. .5x + y <= 3
  //       x + y <= 3
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

TEST(KnitroInterface, ChangeVarIntoInteger) {
  // max   x
  // st.   x + y >= 2.5
  //       x + y >= -2.5
  //       x - y <= 2.5
  //       x - y >= -2.5
  //       x , y \in R
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

  x->SetInteger(true);
  solver.Solve();
  EXPECT_NEAR(obj->Value(), 2, 1e-7);
}

TEST(KnitroInterface, AddVarAndConstraint) {
  // max x + y                max x + y + z
  // st. x , y <= 1;    ->    st. x , y , z >= 0
  //     x , y >= 0;              x , y , z <= 1
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
