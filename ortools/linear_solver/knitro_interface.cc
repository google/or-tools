// Copyright 2019-2023 RTE
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

// Hearder to be written

#include <algorithm>
#include <clocale>
#include <fstream>
#include <istream>
#include <limits>
#include <cmath>
#include <memory>
#include <mutex>
#include <string>

#include "absl/strings/str_format.h"
#include "ortools/base/logging.h"
#include "ortools/base/timer.h"
#include "ortools/linear_solver/linear_solver.h"
#include "ortools/knitro/environment.h"

#define CHECK_STATUS(s)    \
  do {                     \
    int const status_ = s; \
    CHECK_EQ(0, status_);  \
  } while (0)

namespace operations_research {

class KnitroInterface : public MPSolverInterface {
public:
    explicit KnitroInterface(MPSolver* solver, bool mip);
    ~KnitroInterface() override;

    
    MPSolver::ResultStatus Solve(const MPSolverParameters& param) override;

    // TODO : A voir si on override
    // std::optional<MPSolutionResponse> DirectlySolveProto(
    //     const MPModelRequest& request, std::atomic<bool>* interrupt) override;

    void Write(const std::string& filename) override;
    void Reset() override;

    double infinity() override {
        return KN_INFINITY;
    };

    void SetOptimizationDirection(bool maximize) override;
    void SetVariableBounds(int var_index, double lb, double ub) override;
    void SetVariableInteger(int var_index, bool integer) override;
    void SetConstraintBounds(int row_index, double lb, double ub) override;

    void AddRowConstraint(MPConstraint* ct) override;
    void AddVariable(MPVariable* var) override;
    void SetCoefficient(MPConstraint* constraint, const MPVariable* variable,
                        double new_value, double old_value) override;
    void ClearConstraint(MPConstraint* constraint) override;
    void SetObjectiveCoefficient(const MPVariable* variable,
                                double coefficient) override;
    void SetObjectiveOffset(double value) override;
    void ClearObjective() override;
    void BranchingPriorityChangedForVariable(int var_index) override;

    int64_t iterations() const override;
    int64_t nodes() const override;

    MPSolver::BasisStatus row_status(int constraint_index) const override {
        LOG(DFATAL) << "Not Supported by Knitro ! ";
        return MPSolver::FREE;
    }
    MPSolver::BasisStatus column_status(int variable_index) const override {
        LOG(DFATAL) << "Not Supported by Knitro ! ";
        return MPSolver::FREE;
    }

    bool IsContinuous() const override { return !mip_; }
    bool IsLP() const override { return !mip_; }
    bool IsMIP() const override { return mip_; }

    void ExtractNewVariables() override;
    void ExtractNewConstraints() override;
    void ExtractObjective() override;

    std::string SolverVersion() const override;

    void* underlying_solver() override { return reinterpret_cast<void*>(kc_); }

    virtual double ComputeExactConditionNumber() const override {
        LOG(DFATAL) << "ComputeExactConditionNumber not implemented for"
                    << " Knitro Programming";
        return 0.0;
    };

    // TODO
    // bool InterruptSolve() override;
    // bool NextSolution() override;


    // TODO : MPCallback API
    // void SetCallback(MPCallback* mp_callback) override;
    bool SupportsCallbacks() const override { return true; }

private:
    void SetParameters(const MPSolverParameters& param) override;
    void SetRelativeMipGap(double value) override;
    void SetPrimalTolerance(double value) override;
    void SetDualTolerance(double value) override;
    void SetPresolveMode(int presolve) override;
    void SetScalingMode(int scaling) override;
    void SetLpAlgorithm(int lp_algorithm) override;

    // TODO 
    // absl::Status SetNumThreads(int num_threads) override;

    bool SetSolverSpecificParametersAsString(
        const std::string& parameters) override;

    void AddSolutionHintToOptimizer();
    void SetSolution();

    KN_context* kc_;
    bool mip_;
    bool init_solve_;
};

KnitroInterface::KnitroInterface(MPSolver* solver, bool mip)
    : MPSolverInterface(solver), kc_(nullptr), mip_(mip), init_solve_(false) {
    CHECK_STATUS(KN_new(&kc_));
}

KnitroInterface::~KnitroInterface(){
    CHECK_STATUS(KN_free(&kc_));
}

// ------ Model modifications and extraction -----

void KnitroInterface::Reset() {
    // Instead of explicitly clearing all modeling objects we
    // just delete the problem object and allocate a new one.
    CHECK_STATUS(KN_free(&kc_));
    init_solve_ = false;
    int status;
    status = KN_new(&kc_);
    CHECK_STATUS(status);
    DCHECK(kc_ != nullptr);  // should not be NULL if status=0
    ResetExtractionInformation();
}

void KnitroInterface::Write(const std::string& filename) {
  ExtractModel();
  VLOG(1) << "Writing Knitro MPS \"" << filename << "\".";
  const int status = KN_write_mps_file(kc_, filename.c_str());
  if (status) {
    LOG(ERROR) << "Knitro: Failed to write MPS!";
  }
}

void KnitroInterface::SetOptimizationDirection(bool maximize){
    InvalidateSolutionSynchronization();
    CHECK_STATUS(KN_set_obj_goal(kc_, (maximize) ? KN_OBJGOAL_MAXIMIZE : KN_OBJGOAL_MINIMIZE));
}

void KnitroInterface::SetVariableBounds(int var_index, double lb, double ub){
    InvalidateSolutionSynchronization();
    if (variable_is_extracted(var_index)) {
        // Not cached if the variable has been extracted.
        DCHECK_LT(var_index, last_variable_index_);
        CHECK_STATUS(KN_set_var_lobnd(kc_,var_index, lb));
        CHECK_STATUS(KN_set_var_upbnd(kc_,var_index, ub));
    } else {
        sync_status_ = MUST_RELOAD;
    }
}

void KnitroInterface::SetVariableInteger(int var_index, bool integer){
    InvalidateSolutionSynchronization();
    if (mip_){
        if (variable_is_extracted(var_index)){
            DCHECK_LT(var_index, last_variable_index_);
            CHECK_STATUS(KN_set_var_type(kc_, var_index, KN_VARTYPE_INTEGER));            
        } else {
            sync_status_ = MUST_RELOAD;
        }
    } else {
        LOG(DFATAL)  << "Attempt to change variable to integer in non-MIP problem!";
    }
}

void KnitroInterface::SetConstraintBounds(int row_index, double lb, double ub){
    InvalidateSolutionSynchronization();
    if (constraint_is_extracted(row_index)){
        DCHECK_LT(row_index, last_constraint_index_);
        CHECK_STATUS(KN_set_con_lobnd(kc_, row_index, lb));
        CHECK_STATUS(KN_set_con_upbnd(kc_, row_index, ub));
    } else {
        sync_status_ = MUST_RELOAD;
    }
}

void KnitroInterface::SetCoefficient(MPConstraint* constraint, 
                                     const MPVariable* variable,
                                     double new_value, double old_value) {
    InvalidateSolutionSynchronization();
    int var_index = variable->index(), row_index = constraint->index();
    if (variable_is_extracted(var_index) &&
        constraint_is_extracted(row_index)) {
        DCHECK_LT(row_index, last_constraint_index_);
        DCHECK_LT(var_index, last_variable_index_);
        CHECK_STATUS(KN_chg_con_linear_term(kc_, row_index,var_index, new_value));
    } else {
        sync_status_ = MUST_RELOAD;
    }    
}

void KnitroInterface::ClearConstraint(MPConstraint *constraint){
    InvalidateSolutionSynchronization();
    
    int const row = constraint->index();

    if (!constraint_is_extracted(row)) return;

    int const len = constraint->coefficients_.size();
    std::unique_ptr<int[]> rowind(new int[len]);
    std::unique_ptr<double[]> val(new double[len]);
    int j = 0;
    const auto& coeffs = constraint->coefficients_;
    for (auto coeff : coeffs) {
      int const col = coeff.first->index();
      if (variable_is_extracted(col)) {
        rowind[j] = row;
        val[j] = 0.0;
        ++j;
      }
    }
    if (j>0){
      CHECK_STATUS(
        KN_chg_con_linear_struct_one(kc_, j, constraint->index(), rowind.get(), 
                                     val.get())
        );        
    }

}

void KnitroInterface::SetObjectiveCoefficient(const MPVariable* variable, double coefficient){
    sync_status_ = MUST_RELOAD;
}

void KnitroInterface::SetObjectiveOffset(double value){
    sync_status_ = MUST_RELOAD;
}

void KnitroInterface::ClearObjective(){
    // return without clearing the objective coef if 
    // non initial solve has been done before
    if (!init_solve_) return;
    InvalidateSolutionSynchronization();
    int const cols = solver_->objective_->coefficients_.size();
    std::unique_ptr<int[]> ind(new int[cols]);
    int j = 0;
    const auto& coeffs = solver_->objective_->coefficients_;
    for (auto coeff : coeffs) {
      int const idx = coeff.first->index();
      // We only need to reset variables that have been extracted.
      if (variable_is_extracted(idx)) {
        DCHECK_LT(idx, cols);
        ind[j] = idx;
        ++j;
      }
    }
    if (j > 0) {
      CHECK_STATUS(KN_del_obj_linear_struct(kc_, j, ind.get()));
    }
    CHECK_STATUS(KN_del_obj_constant(kc_));
}

void KnitroInterface::BranchingPriorityChangedForVariable(int var_index){
    InvalidateSolutionSynchronization();
    if (mip_){
        if (variable_is_extracted(var_index)){
            DCHECK_LT(var_index, last_variable_index_);
            int const priority = solver_->variables_[var_index]->branching_priority();
            CHECK_STATUS(KN_set_mip_branching_priority(kc_, var_index, priority));            
        } else {
            sync_status_ = MUST_RELOAD;
        }
    } else {
        LOG(DFATAL)  << "Attempt to change branching priority of variable in non-MIP problem!";
    }
}

void KnitroInterface::AddRowConstraint(MPConstraint* ct) {
  sync_status_ = MUST_RELOAD;
}

void KnitroInterface::AddVariable(MPVariable* var) { 
    sync_status_ = MUST_RELOAD; 
}

// Knitro does not support inf value so it is mandatory to convert it to KN_INFINITY
double redefine_infinity_double(double value){
    if (isinf(value)){
        return value > 0 ? KN_INFINITY : - KN_INFINITY;
    }
    return value;
}

void KnitroInterface::ExtractNewVariables() {
    int const total_num_vars = solver_->variables_.size();
    if (total_num_vars > last_variable_index_) {
        int const len = total_num_vars-last_variable_index_;
        // for init and def basic properties
        std::unique_ptr<int[]> idx_vars(new int[len]);
        std::unique_ptr<double[]> lb(new double[len]);
        std::unique_ptr<double[]> ub(new double[len]);
        std::unique_ptr<int[]> types(new int[len]);
        // lambda fn to destroy the array of names
        auto deleter=[len](char** ptr){            
            for (int i = 0; i < len; ++i) {
                    delete[] ptr[i];
                }
            delete[] ptr;
        };
        std::unique_ptr<char*[], decltype(deleter)> names(new char*[len],
                                                            deleter);
        // for priority properties
        std::unique_ptr<int[]> prior(new int[len]);
        std::unique_ptr<int[]> prior_idx(new int[len]);
        int prior_nb=0;
        // Define new variables
        for (int j = 0, var_index = last_variable_index_; j < len; ++j, ++var_index) {
            MPVariable* const var = solver_->variables_[var_index];
            DCHECK(!variable_is_extracted(var_index));
            set_variable_as_extracted(var_index, true);
            idx_vars[j] = var_index;
            lb[j] = redefine_infinity_double(var->lb());
            ub[j] = redefine_infinity_double(var->ub());
            // Def buffer size at 256 for variables' name
            names[j] = new char[256];
            strcpy(names[j],var->name().c_str());
            types[j] = var->integer() ? KN_VARTYPE_INTEGER : KN_VARTYPE_CONTINUOUS;
            if (var->integer() && (var->branching_priority()!=0)) {
                prior_idx[prior_nb] = var_index;
                prior[prior_nb] = var->branching_priority();
                prior_nb++;
            }
        }
        CHECK_STATUS(KN_add_vars(kc_, len, NULL));
        CHECK_STATUS(KN_set_var_lobnds(kc_, len, idx_vars.get(), lb.get()));
        CHECK_STATUS(KN_set_var_upbnds(kc_, len, idx_vars.get(), ub.get()));
        CHECK_STATUS(KN_set_var_types(kc_, len, idx_vars.get(), types.get()));
        CHECK_STATUS(KN_set_var_names(kc_, len, idx_vars.get(), names.get()));
        CHECK_STATUS(
            KN_set_mip_branching_priorities(kc_, prior_nb, 
                                            prior_idx.get(), prior.get())
        );

    }
}

void KnitroInterface::ExtractNewConstraints() {
    int const total_num_cons = solver_->constraints_.size();
    int const total_num_vars = solver_->variables_.size();
    if (total_num_cons > last_constraint_index_) {
        int const len = total_num_cons-last_constraint_index_;
        std::unique_ptr<int[]> idx_cons(new int[len]);
        std::unique_ptr<double[]> lb(new double[len]);
        std::unique_ptr<double[]> ub(new double[len]);
        std::unique_ptr<int[]> lin_idx_cons(new int[len*total_num_vars]);
        std::unique_ptr<int[]> lin_idx_vars(new int[len*total_num_vars]);
        std::unique_ptr<double[]> lin_coefs(new double[len*total_num_vars]);
        // lambda fn to destroy the array of names
        auto deleter=[len](char** ptr){            
            for (int i = 0; i < len; ++i) {
                    delete[] ptr[i];
                }
            delete[] ptr;
        };
        std::unique_ptr<char*[], decltype(deleter)> names(new char*[len],
                                                            deleter);
        int idx_lin_term=0;
        //Define new constraints
        for (int j=0, con_index=last_constraint_index_; j<len; ++j, ++con_index){
            MPConstraint* const ct = solver_->constraints_[con_index];
            DCHECK(!constraint_is_extracted(con_index));
            set_constraint_as_extracted(con_index, true);
            idx_cons[j] = con_index;
            lb[j] = redefine_infinity_double(ct->lb());
            ub[j] = redefine_infinity_double(ct->ub());
            for (int i=0; i<total_num_vars; ++i){
                lin_idx_cons[idx_lin_term] = con_index;
                lin_idx_vars[idx_lin_term] = i;
                lin_coefs[idx_lin_term] = ct->GetCoefficient(solver_->variables_[i]);
                idx_lin_term++;
            }
            // Def buffer size at 256 for variables' name
            names[j] = new char[256];
            strcpy(names[j],ct->name().c_str());
        }
        CHECK_STATUS(KN_add_cons(kc_, len, NULL));
        CHECK_STATUS(KN_set_con_lobnds(kc_, len, idx_cons.get(), lb.get()));
        CHECK_STATUS(KN_set_con_upbnds(kc_, len, idx_cons.get(), ub.get()));
        CHECK_STATUS(
            KN_add_con_linear_struct(kc_, idx_lin_term, lin_idx_cons.get(), 
                                     lin_idx_vars.get(), lin_coefs.get())
        );
        CHECK_STATUS(KN_set_con_names(kc_, len, idx_cons.get(), names.get()));
    }
}

void KnitroInterface::ExtractObjective() {

    int const len = solver_->variables_.size();

    std::unique_ptr<int[]> ind(new int[len]);
    std::unique_ptr<double[]> val(new double[len]);
    for (int j = 0; j < len; ++j) {
        ind[j] = j;
        val[j] = 0.0;
    }

    const auto& coeffs = solver_->objective_->coefficients_;
    for (auto coeff : coeffs) {
        int const idx = coeff.first->index();
        if (variable_is_extracted(idx)) {
        DCHECK_LT(idx, len);
        ind[idx] = idx;
        val[idx] = coeff.second;
        }
    }

    if (init_solve_){ // if a init solve occured, remove prev coef to add the new ones
        CHECK_STATUS(KN_chg_obj_linear_struct(kc_, len, ind.get(), val.get()));
        CHECK_STATUS(KN_chg_obj_constant(kc_, solver_->Objective().offset()));
    } else {
        CHECK_STATUS(KN_add_obj_linear_struct(kc_, len, ind.get(), val.get()));
        CHECK_STATUS(KN_add_obj_constant(kc_, solver_->Objective().offset()));
    }
    // Extra check on the optimization direction
    SetOptimizationDirection(maximize_);
}

// ------ Parameters  -----

void KnitroInterface::SetParameters(const MPSolverParameters& param) {
  SetCommonParameters(param);
  // TODO : à verif
  SetScalingMode(param.GetIntegerParam(MPSolverParameters::SCALING));
  if (mip_) SetMIPParameters(param);
}

bool KnitroInterface::SetSolverSpecificParametersAsString(
        const std::string& parameters) {
        if (parameters.empty()) {
            return true;
        } 
        if (KN_load_param_file(kc_, parameters.c_str())==0){
            return true;
        }
        return false;
}

void KnitroInterface::SetRelativeMipGap(double value) {
  if (mip_) {
    CHECK_STATUS(KN_set_double_param(kc_, KN_PARAM_MIP_OPTGAPREL, value));
  } else {
    LOG(WARNING) << "The relative MIP gap is only available "
                 << "for discrete problems.";
  }
}

void KnitroInterface::SetPrimalTolerance(double value) {
  CHECK_STATUS(KN_set_double_param(kc_, KN_PARAM_FEASTOL, value));
}

void KnitroInterface::SetDualTolerance(double value) {
  CHECK_STATUS(KN_set_double_param(kc_, KN_PARAM_OPTTOL, value));
}

void KnitroInterface::SetPresolveMode(int value) {
  auto const presolve = static_cast<MPSolverParameters::PresolveValues>(value);

  switch (presolve) {
    case MPSolverParameters::PRESOLVE_OFF:
      CHECK_STATUS(KN_set_int_param(kc_, KN_PARAM_PRESOLVE, KN_PRESOLVE_NO));
      return;
    case MPSolverParameters::PRESOLVE_ON:
      CHECK_STATUS(KN_set_int_param(kc_, KN_PARAM_PRESOLVE, KN_PRESOLVE_YES));
      return;
  }
  SetIntegerParamToUnsupportedValue(MPSolverParameters::PRESOLVE, value);
}

void KnitroInterface::SetScalingMode(int value) {
  auto const scaling = static_cast<MPSolverParameters::ScalingValues>(value);

  switch (scaling) {
    case MPSolverParameters::SCALING_OFF:
      CHECK_STATUS(KN_set_int_param(kc_, KN_PARAM_LINSOLVER_SCALING, KN_LINSOLVER_SCALING_NONE));
      break;
    case MPSolverParameters::SCALING_ON:
      CHECK_STATUS(KN_set_int_param(kc_, KN_PARAM_LINSOLVER_SCALING, KN_LINSOLVER_SCALING_ALWAYS));
      break;
  }
}

void KnitroInterface::SetLpAlgorithm(int value) {
    auto const algorithm =
        static_cast<MPSolverParameters::LpAlgorithmValues>(value);
    switch (algorithm) {
        case MPSolverParameters::PRIMAL:
            CHECK_STATUS(KN_set_int_param(kc_, KN_PARAM_ACT_LPALG, KN_ACT_LPALG_PRIMAL));
            break;
        case MPSolverParameters::DUAL:
            CHECK_STATUS(KN_set_int_param(kc_, KN_PARAM_ACT_LPALG, KN_ACT_LPALG_DUAL));
            break;
        case MPSolverParameters::BARRIER:
            CHECK_STATUS(KN_set_int_param(kc_, KN_PARAM_ACT_LPALG, KN_ACT_LPALG_BARRIER));
            break;        
        default:
            CHECK_STATUS(KN_set_int_param(kc_, KN_PARAM_ACT_LPALG, KN_ACT_LPALG_DEFAULT));
            break;
    }
}

// ------ Solve  -----

MPSolver::ResultStatus KnitroInterface::Solve(MPSolverParameters const& param) {

    WallTimer timer;
    timer.Start();

    if (param.GetIntegerParam(MPSolverParameters::INCREMENTALITY) ==
        MPSolverParameters::INCREMENTALITY_OFF) {
        Reset();
    }

    ExtractModel();
    VLOG(1) << absl::StrFormat("Model build in %.3f seconds.", timer.Get());

    if (quiet_){
        // Silence the screen output : will append into knitro.log file
        CHECK_STATUS(KN_set_int_param(kc_, KN_PARAM_OUTLEV, KN_OUTLEV_NONE));
    }


    // Set parameters.
    solver_->SetSolverSpecificParametersAsString(
        solver_->solver_specific_parameter_string_);
    SetParameters(param);
    if (solver_->time_limit()) {
        VLOG(1) << "Setting time limit = " << solver_->time_limit() << " ms.";
        CHECK_STATUS(KN_set_int_param(kc_, KN_PARAM_MAXTIMECPU,
                                      solver_->time_limit_in_secs()));
    }

    // // Load basis if present
    // // TODO : check number of variables / constraints
    // if (!mip_ && !initial_variables_basis_status_.empty() &&
    //     !initial_constraint_basis_status_.empty()) {
    //     CHECK_STATUS(XPRSloadbasis(mLp, initial_constraint_basis_status_.data(),
    //                             initial_variables_basis_status_.data()));
    // }

    // Set the hint (if any)
    this->AddSolutionHintToOptimizer();

    // // TODO : Add opt node callback to optimizer. We have to do this here (just before
    // // solve) to make sure the variables are fully initialized
    // MPCallbackWrapper* mp_callback_wrapper = nullptr;
    // if (callback_ != nullptr) {
    //     mp_callback_wrapper = new MPCallbackWrapper(callback_);
    //     CHECK_STATUS(XPRSaddcbintsol(mLp, XpressIntSolCallbackImpl,
    //                                 static_cast<void*>(mp_callback_wrapper), 0));
    // }

    // Solve.
    timer.Restart();
    int status;

    // check for multi-thread solve
    status = KN_solve(kc_);

    if (status == 0){ 
        //the final solution is optimal to specified tolerances;
        result_status_ = MPSolver::OPTIMAL;
    } else if (status < 110 && 100 <= status){
        //a feasible solution was found (but not verified optimal);
        result_status_ = MPSolver::FEASIBLE;
    } else if (status < 210 && 200 <= status){
        //Knitro terminated at an infeasible point;
        result_status_ = MPSolver::INFEASIBLE;
    } else if (status < 302 && 300 <= status){
        //the problem was determined to be unbounded;
        result_status_ = MPSolver::UNBOUNDED;
    } else if (status < 410 && 400 <= status){
        //a feasible point was found before reaching the limit
        result_status_ = MPSolver::FEASIBLE;
    } else if (status < 420 && 410 <= status){
        //no feasible point was found before reaching the limit
        result_status_ = MPSolver::INFEASIBLE;
    } else {
        //Knitro terminated with an input error or some non-standard error or else.
        result_status_ = MPSolver::ABNORMAL;
    }
    
    if (result_status_ == MPSolver::OPTIMAL || result_status_ == MPSolver::FEASIBLE) {
        // If optimal or feasible solution is found.
        SetSolution();
    } else {
        VLOG(1) << "No feasible solution found.";
    }

    sync_status_ = SOLUTION_SYNCHRONIZED;
    init_solve_ = true;
    return result_status_;
}

void KnitroInterface::SetSolution(){
    int status;
    int const nb_vars = solver_-> variables_.size();
    int const nb_cons = solver_-> constraints_.size();
    if (nb_vars){
        std::unique_ptr<double[]> values(new double[nb_vars]);
        std::unique_ptr<double[]> reduced_costs(new double[nb_vars]);        
        CHECK_STATUS(KN_get_solution(kc_, &status, &objective_value_, values.get(), NULL));
        CHECK_STATUS(KN_get_var_dual_values_all(kc_, reduced_costs.get()));
        for (int j=0; j<nb_vars; ++j){
            MPVariable* var = solver_->variables_[j];
            var->set_solution_value(values[j]);
            if (!mip_) var->set_reduced_cost(reduced_costs[j]); // TODO : check if the sign is needed
        }    
    }
    if (nb_cons){
        std::unique_ptr<double[]> duals_cons(new double[nb_cons]);
        CHECK_STATUS(KN_get_con_dual_values_all(kc_, duals_cons.get()));
        if (!mip_){
            for (int j=0; j<nb_cons; ++j){
                MPConstraint* ct = solver_->constraints_[j];
                ct->set_dual_value(duals_cons[j]); // TODO : check if the sign is needed
            }  
        }     
    }
   
}

void KnitroInterface::AddSolutionHintToOptimizer() {
    const std::size_t len = solver_->solution_hint_.size();
    if (len == 0) {
        // hint is empty, nothing to do
        return;
    }
    std::unique_ptr<int[]> col_ind(new int[len]);
    std::unique_ptr<double[]> val(new double[len]);

    for (std::size_t i = 0; i < len; ++i) {
        col_ind[i] = solver_->solution_hint_[i].first->index();
        val[i] = solver_->solution_hint_[i].second;
    }
    CHECK_STATUS(KN_set_var_primal_init_values(kc_, len, col_ind.get(), val.get()));
}

// ------ Query statistics on the solution and the solve ------

int64_t KnitroInterface::iterations() const {
  if (!CheckSolutionIsSynchronized()) return kUnknownNumberOfIterations;
  int numIters;
  CHECK_STATUS(KN_get_number_iters(kc_, &numIters));
  return static_cast<int64_t>(numIters);
}

int64_t KnitroInterface::nodes() const {
  if (mip_) {
    if (!CheckSolutionIsSynchronized()) return kUnknownNumberOfNodes;
    int numNodes;
    CHECK_STATUS(KN_get_mip_number_nodes(kc_, &numNodes));
    return static_cast<int64_t>(numNodes);
  } else {
    LOG(DFATAL) << "Number of nodes only available for discrete problems";
    return kUnknownNumberOfNodes;
  }
}

// ----- Misc -----

std::string KnitroInterface::SolverVersion() const {
    int const length = 15; // should contain the string termination character
    char release[length+1]; // checked but there are trouble if not allocated with a additional char

    CHECK_STATUS(KN_get_release(length, release));

    return absl::StrFormat("Knitro library version %s", release);
}

MPSolverInterface* BuildKnitroInterface(bool mip, MPSolver* const solver) {
    return new KnitroInterface(solver, mip);
}

}  // namespace operations_research