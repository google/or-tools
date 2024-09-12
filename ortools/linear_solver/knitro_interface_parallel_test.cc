// ADD HEADER
#include <stdio.h>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
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

#define MOCK_MIP()                                                           \
  MPSolver solver("KNITRO_MIP", MPSolver::KNITRO_MIXED_INTEGER_PROGRAMMING); \
  KnitroGetter getter(&solver)
#define MOCK_LP()                                                    \
  MPSolver solver("KNITRO_LP", MPSolver::KNITRO_LINEAR_PROGRAMMING); \
  KnitroGetter getter(&solver)

inline void set_problem(MPSolver& solver, bool mip) {
  double infinity = solver.infinity();
  MPVariable* const x =
      (mip) ? solver.MakeIntVar(0, 7, "x") : solver.MakeNumVar(0, 7, "x");
  MPVariable* const y =
      (mip) ? solver.MakeIntVar(0, 6, "y") : solver.MakeNumVar(0, 6, "y");
  MPConstraint* const c1 = solver.MakeRowConstraint(-infinity, 9, "c1");
  c1->SetCoefficient(x, 1);
  c1->SetCoefficient(y, 1);
  MPConstraint* const c2 = solver.MakeRowConstraint(-infinity, 18, "c2");
  c2->SetCoefficient(x, 3);
  c2->SetCoefficient(y, 1);
  MPObjective* const obj = solver.MutableObjective();
  obj->SetCoefficient(x, 3);
  obj->SetCoefficient(y, 2);
  obj->SetMaximization();
}

#define CHECK_MIP()                                     \
  EXPECT_EQ(solver.variable(0)->solution_value(), 4);   \
  EXPECT_EQ(solver.variable(1)->solution_value(), 5);   \
  EXPECT_EQ(solver.MutableObjective()->Value(), 22);


#define CHECK_LP()                                                    \
  EXPECT_NEAR(solver.variable(0)->solution_value(), 4.5, ERROR_RATE); \
  EXPECT_NEAR(solver.variable(1)->solution_value(), 4.5, ERROR_RATE); \
  EXPECT_NEAR(solver.MutableObjective()->Value(), 22.5, ERROR_RATE);


// -------------------- Parallel Misc Test --------------------

/** Unit Test of the method SetNumThreads()*/
TEST(KnitroInterface, SetNumThreads) {
  MOCK_MIP();
  set_problem(solver, true);
  auto status = solver.SetNumThreads(4);
  solver.Solve();
  CHECK_MIP();
  int value;
  getter.Int_Param(KN_PARAM_NUMTHREADS, &value);
  EXPECT_EQ(value, 4);
}

/** Unit Test to check the Parallel BLAS*/
TEST(KnitroInterface, PBLAS) {
  MOCK_MIP();
  set_problem(solver, true);
  solver.SetSolverSpecificParametersAsString("KN_PARAM_BLASOPTION 1 KN_PARAM_BLAS_NUMTHREADS 4");
  solver.Solve();
  CHECK_MIP();
  int value;
  getter.Int_Param(KN_PARAM_BLASOPTION, &value);
  EXPECT_EQ(value, 1);
  getter.Int_Param(KN_PARAM_BLAS_NUMTHREADS, &value);
  EXPECT_EQ(value, 4);
}

// TODO : KN_PARAM_CONCURRENT_EVALS -> Callback
// TODO : KN_PARAM_CONIC_NUMTHREADS (not used)
// TODO : KN_PARAM_FINDIFF_NUMTHREADS (not used?)

/** Unit Test to modify linsolver_numthreads*/
TEST(KnitroInterface, linsolver_numthreads) {
  MOCK_MIP();
  set_problem(solver, true);
  solver.SetSolverSpecificParametersAsString("KN_PARAM_LINSOLVER 6 KN_PARAM_LINSOLVER_NUMTHREADS 4");
  solver.Solve();
  CHECK_MIP();
  int value;
  getter.Int_Param(KN_PARAM_LINSOLVER, &value);
  EXPECT_EQ(value, 6);
  getter.Int_Param(KN_PARAM_LINSOLVER_NUMTHREADS, &value);
  EXPECT_EQ(value, 4);
}

/** Unit Test to modify mip_numthreads*/
TEST(KnitroInterface, mip_numthreads) {
  MOCK_MIP();
  set_problem(solver, true);
  solver.SetSolverSpecificParametersAsString("KN_PARAM_MIP_METHOD 1 KN_PARAM_MIP_NUMTHREADS 4");
  solver.Solve();
  CHECK_MIP();
  int value;
  getter.Int_Param(KN_PARAM_MIP_METHOD, &value);
  EXPECT_EQ(value, 1);
  getter.Int_Param(KN_PARAM_MIP_NUMTHREADS, &value);
  EXPECT_EQ(value, 4);
}

// -------------------- Multi-start --------------------

/** 
 * Functionnal Test for multi-start
 * Just test some of the different parameters
 * of multi-start option in Knitro
 */
TEST(KnitroInterface, multistart) {
  MOCK_LP();
  double infinity = solver.infinity();
  MPVariable* const x = solver.MakeNumVar(0, 7, "x");
  MPVariable* const y = solver.MakeNumVar(0, 6, "y");
  MPConstraint* const c1 = solver.MakeRowConstraint(-infinity, 9, "c1");
  c1->SetCoefficient(x, 1);
  c1->SetCoefficient(y, 1);
  MPConstraint* const c2 = solver.MakeRowConstraint(-infinity, 18, "c2");
  c2->SetCoefficient(x, 3);
  c2->SetCoefficient(y, 1);
  MPObjective* const obj = solver.MutableObjective();
  obj->SetCoefficient(x, 3);
  obj->SetCoefficient(y, 1);
  obj->SetMaximization();
  solver.SetSolverSpecificParametersAsString("KN_PARAM_OUTLEV 1 KN_PARAM_MS_ENABLE 1 KN_PARAM_MS_MAXSOLVES 16 KN_PARAM_MS_NUMTOSAVE 20 KN_PARAM_MS_MAXTIMECPU 1e6 KN_PARAM_MS_MAXTIMEREAL 1e6 KN_PARAM_MS_SAVETOL 1e-9 KN_PARAM_MS_NUMTHREADS 4");
  solver.Solve();
  EXPECT_NEAR(obj->Value(), 18, ERROR_RATE);
  int value;
  getter.Int_Param(KN_PARAM_MS_ENABLE, &value);
  EXPECT_EQ(value, 1);
  getter.Int_Param(KN_PARAM_MS_MAXSOLVES, &value);
  EXPECT_EQ(value, 16);
  getter.Int_Param(KN_PARAM_MS_NUMTOSAVE, &value);
  EXPECT_EQ(value, 20);
  getter.Int_Param(KN_PARAM_MS_NUMTHREADS, &value);
  EXPECT_EQ(value, 4);
  double value2;
  getter.Double_Param(KN_PARAM_MS_MAXTIMEREAL, &value2);
  EXPECT_EQ(value2, 1e6);
  getter.Double_Param(KN_PARAM_MS_MAXTIMECPU, &value2);
  EXPECT_EQ(value2, 1e6);
  getter.Double_Param(KN_PARAM_MS_SAVETOL, &value2);
  EXPECT_EQ(value2, 1e-9);
}

// -------------------- Tuner --------------------

/** 
 * Functionnal Test for Knitro-tuner
 * Just test some of the different parameters
 * of tuner option in Knitro
 */
TEST(KnitroInterface, KnitroTuner) {
  MOCK_LP();
  set_problem(solver, false);
  std::ofstream tuner_file;
  tuner_file.open("knitro_interface_tuner_settings.opt");
  tuner_file << "KN_PARAM_ALG\n";
  tuner_file << "KN_PARAM_FEASTOL 1e-8 1e-10`\n";
  tuner_file.close();

  solver.SetSolverSpecificParametersAsString("KN_PARAM_OUTLEV 1 KN_PARAM_TUNER 1 KN_PARAM_TUNER_OPTIONSFILE knitro_interface_tuner_settings.opt KN_PARAM_TUNER_OUTSUB 1");
  solver.Solve();
  CHECK_LP();
  remove("knitro_interface_tuner_settings.opt");
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
      return EXIT_SUCCESS;
    }
    return EXIT_FAILURE;
  }
}
