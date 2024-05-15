// ADD HEADER

#include <algorithm>
#include <clocale>
#include <cmath>
#include <fstream>
#include <istream>
#include <limits>
#include <memory>
#include <mutex>
#include <string>

#include "absl/status/status.h"
#include "absl/strings/str_format.h"
#include "ortools/base/logging.h"
#include "ortools/base/timer.h"
#include "ortools/knitro/environment.h"
#include "ortools/linear_solver/linear_solver.h"

#define CHECK_STATUS(s)    \
  do {                     \
    int const status_ = s; \
    CHECK_EQ(0, status_);  \
  } while (0)

namespace operations_research {

/**
 * Knitro does not support inf values
 * so it is mandatory to convert them into
 * KN_INFINITY
 * @param value the evaluated value
 * @return KN_INFINITY if the value is inf otherwise value
 */
inline double redefine_infinity_double(double value) {
  if (std::isinf(value)) {
    return value > 0 ? KN_INFINITY : -KN_INFINITY;
  }
  return value;
}

/*------------KnitroInterface Definition------------*/

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

  double infinity() override { return KN_INFINITY; };

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

  void SetCallback(MPCallback* mp_callback) override;
  bool SupportsCallbacks() const override { return true; }

 private:
  void SetParameters(const MPSolverParameters& param) override;
  void SetRelativeMipGap(double value) override;
  void SetPrimalTolerance(double value) override;
  void SetDualTolerance(double value) override;
  void SetPresolveMode(int presolve) override;
  void SetScalingMode(int scaling) override;
  void SetLpAlgorithm(int lp_algorithm) override;

  absl::Status SetNumThreads(int num_threads) override;

  bool SetSolverSpecificParametersAsString(
      const std::string& parameters) override;

  void AddSolutionHintToOptimizer();
  void SetSolution();

  KN_context* kc_;
  bool mip_;
  bool no_obj_;
  MPCallback* callback_ = nullptr;
};

/*------------Knitro CallBack Context------------*/

/**
 * Knitro's MPCallbackContext derived class
 * 
 * Stores the values x and lambda provided by Knitro MIP Callback functions
 * eventhough lambda can't be used with the current MPCallbackContext definition
 * 
 * Return code from Knitro solver's cuts can't be retrieved neither
 */
class KnitroMPCallbackContext : public MPCallbackContext {
  friend class KnitroInterface;

 public:
  KnitroMPCallbackContext(KN_context_ptr* kc, MPCallbackEvent event,
                          const double* const x, const double* const lambda)
      : kc_ptr_(kc), event_(event), var_val(x), lambda(lambda){};

  // Implementation of the interface.
  MPCallbackEvent Event() override { return event_; };
  bool CanQueryVariableValues() override;
  double VariableValue(const MPVariable* variable) override;

  // Knitro supports cuts and lazy constraints only
  void AddCut(const LinearRange& cutting_plane) override;
  void AddLazyConstraint(const LinearRange& lazy_constraint) override;
  double SuggestSolution(
      const absl::flat_hash_map<const MPVariable*, double>& solution) override {
    LOG(WARNING) << "SuggestSolution is not implemented in Knitro interface";
    return NAN;
  }

  int64_t NumExploredNodes() override;

 private:
  KN_context_ptr* kc_ptr_;
  MPCallbackEvent event_;
  const double* const var_val;
  // lambda is not used
  const double* const lambda;
};

bool KnitroMPCallbackContext::CanQueryVariableValues() {
  switch (event_) {
    case MPCallbackEvent::kMipSolution:
    case MPCallbackEvent::kMipNode:
      return true;
    default:
      return false;
  }
}

double KnitroMPCallbackContext::VariableValue(const MPVariable* variable) {
  return var_val[variable->index()];
}

int64_t KnitroMPCallbackContext::NumExploredNodes() {
  int num_nodes;
  CHECK_STATUS(KN_get_mip_number_nodes(*kc_ptr_, &num_nodes));
  return num_nodes;
}

/**
 * Constraint generator for callback methods.
 * Add new linear constraint to Knitro model as Knitro
 * generate cuts from lazy constraints using the same method.
 * @param kn the Knitro model
 * @param linear_range the constraint
 */
void GenerateConstraint(KN_context* kc, const LinearRange& linear_range) {
  std::vector<int> variable_indices;
  std::vector<double> variable_coefficients;
  const int num_terms = linear_range.linear_expr().terms().size();
  variable_indices.reserve(num_terms);
  variable_coefficients.reserve(num_terms);
  for (const auto& var_coef_pair : linear_range.linear_expr().terms()) {
    variable_indices.push_back(var_coef_pair.first->index());
    variable_coefficients.push_back(var_coef_pair.second);
  }
  int cb_con;
  CHECK_STATUS(KN_add_con(kc, &cb_con));
  CHECK_STATUS(KN_set_con_lobnd(
      kc, cb_con, redefine_infinity_double(linear_range.lower_bound())));
  CHECK_STATUS(KN_set_con_upbnd(
      kc, cb_con, redefine_infinity_double(linear_range.upper_bound())));
  CHECK_STATUS(KN_add_con_linear_struct_one(kc, num_terms, cb_con,
                                            variable_indices.data(),
                                            variable_coefficients.data()));
}

void KnitroMPCallbackContext::AddCut(const LinearRange& cutting_plane) {
  CHECK(event_ == MPCallbackEvent::kMipNode);
  GenerateConstraint(*kc_ptr_, cutting_plane);
}

void KnitroMPCallbackContext::AddLazyConstraint(
    const LinearRange& lazy_constraint) {
  CHECK(event_ == MPCallbackEvent::kMipNode ||
        event_ == MPCallbackEvent::kMipSolution);
  GenerateConstraint(*kc_ptr_, lazy_constraint);
}

struct MPCallBackWithKnitroContext {
  KnitroMPCallbackContext* context;
  MPCallback* callback;
};

struct MPCallBackWithEvent {
  MPCallbackEvent event;
  MPCallback* callback;
};

/**
 * Call-back called by Knitro that needs this type signature.
 */
int KNITRO_API CallBackFn(KN_context_ptr kc, const double* const x,
                          const double* const lambda, void* const userParams) {
  MPCallBackWithEvent* const callback_with_event =
      static_cast<MPCallBackWithEvent*>(userParams);
  CHECK(callback_with_event != nullptr);
  std::unique_ptr<KnitroMPCallbackContext> cb_context;
  cb_context = std::make_unique<KnitroMPCallbackContext>(
      &kc, callback_with_event->event, x, lambda);
  callback_with_event->callback->RunCallback(cb_context.get());
  return 0;
}

/*------------Knitro Interface Implem ------------*/

KnitroInterface::KnitroInterface(MPSolver* solver, bool mip)
    : MPSolverInterface(solver), kc_(nullptr), mip_(mip), no_obj_(true) {
  CHECK_STATUS(KN_new(&kc_));
}

/**
 * Cleans the Knitro problem using Knitro free method.
 */
KnitroInterface::~KnitroInterface() { CHECK_STATUS(KN_free(&kc_)); }

// ------ Model modifications and extraction -----

void KnitroInterface::Reset() {
  // Instead of explicitly clearing all model objects we
  // just delete the problem object and allocate a new one.
  CHECK_STATUS(KN_free(&kc_));
  no_obj_ = true;
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

void KnitroInterface::SetOptimizationDirection(bool maximize) {
  InvalidateSolutionSynchronization();
  CHECK_STATUS(KN_set_obj_goal(
      kc_, (maximize) ? KN_OBJGOAL_MAXIMIZE : KN_OBJGOAL_MINIMIZE));
}

void KnitroInterface::SetVariableBounds(int var_index, double lb, double ub) {
  InvalidateSolutionSynchronization();
  // the "extracted" check is done upstream for now
  // but it should be done here
  if (variable_is_extracted(var_index)) {
    // Not cached if the variable has been extracted.
    DCHECK_LT(var_index, last_variable_index_);
    CHECK_STATUS(
        KN_set_var_lobnd(kc_, var_index, redefine_infinity_double(lb)));
    CHECK_STATUS(
        KN_set_var_upbnd(kc_, var_index, redefine_infinity_double(ub)));
  } else {
    sync_status_ = MUST_RELOAD;
  }
}

void KnitroInterface::SetVariableInteger(int var_index, bool integer) {
  InvalidateSolutionSynchronization();
  if (mip_) {
    // the "extracted" check is done upstream for now
    // but it should be done here
    if (variable_is_extracted(var_index)) {
      DCHECK_LT(var_index, last_variable_index_);
      CHECK_STATUS(KN_set_var_type(
          kc_, var_index,
          integer ? KN_VARTYPE_INTEGER : KN_VARTYPE_CONTINUOUS));
    } else {
      sync_status_ = MUST_RELOAD;
    }
  } else {
    LOG(DFATAL) << "Attempt to change variable to integer in non-MIP problem!";
  }
}

void KnitroInterface::SetConstraintBounds(int row_index, double lb, double ub) {
  InvalidateSolutionSynchronization();
  // the "extracted" check is done upstream for now
  // but it should be done here
  if (constraint_is_extracted(row_index)) {
    DCHECK_LT(row_index, last_constraint_index_);
    CHECK_STATUS(
        KN_set_con_lobnd(kc_, row_index, redefine_infinity_double(lb)));
    CHECK_STATUS(
        KN_set_con_upbnd(kc_, row_index, redefine_infinity_double(ub)));
  } else {
    sync_status_ = MUST_RELOAD;
  }
}

void KnitroInterface::SetCoefficient(MPConstraint* constraint,
                                     const MPVariable* variable,
                                     double new_value, double old_value) {
  InvalidateSolutionSynchronization();
  int var_index = variable->index(), row_index = constraint->index();
  if (variable_is_extracted(var_index) && constraint_is_extracted(row_index)) {
    DCHECK_LT(row_index, last_constraint_index_);
    DCHECK_LT(var_index, last_variable_index_);
    CHECK_STATUS(KN_chg_con_linear_term(kc_, row_index, var_index, new_value));
    CHECK_STATUS(KN_update(kc_));
  } else {
    sync_status_ = MUST_RELOAD;
  }
}

void KnitroInterface::ClearConstraint(MPConstraint* constraint) {
  InvalidateSolutionSynchronization();

  int const row = constraint->index();

  if (!constraint_is_extracted(row)) return;

  int const len = constraint->coefficients_.size();
  std::unique_ptr<int[]> var_ind(new int[len]);
  int j = 0;
  const auto& coeffs = constraint->coefficients_;
  for (auto coeff : coeffs) {
    int const col = coeff.first->index();
    // if the variable has been extracted,
    // then its linear coefficient exists
    if (variable_is_extracted(col)) {
      var_ind[j] = col;
      ++j;
    }
  }
  if (j > 0) {
    // delete all coefficients of constraint's linear structure
    CHECK_STATUS(KN_del_con_linear_struct_one(kc_, j, row, var_ind.get()));
    CHECK_STATUS(KN_update(kc_));
  }
}

void KnitroInterface::SetObjectiveCoefficient(const MPVariable* variable,
                                              double coefficient) {
  sync_status_ = MUST_RELOAD;
}

void KnitroInterface::SetObjectiveOffset(double value) {
  sync_status_ = MUST_RELOAD;
}

void KnitroInterface::ClearObjective() {
  // if the model does not have objective, return
  if (no_obj_) return;
  if (solver_->Objective().offset()) CHECK_STATUS(KN_del_obj_constant(kc_));
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
    CHECK_STATUS(KN_update(kc_));
  }
  no_obj_ = true;
}

void KnitroInterface::BranchingPriorityChangedForVariable(int var_index) {
  InvalidateSolutionSynchronization();
  if (mip_) {
    if (variable_is_extracted(var_index)) {
      DCHECK_LT(var_index, last_variable_index_);
      int const priority = solver_->variables_[var_index]->branching_priority();
      CHECK_STATUS(KN_set_mip_branching_priority(kc_, var_index, priority));
    } else {
      sync_status_ = MUST_RELOAD;
    }
  } else {
    LOG(DFATAL) << "Attempt to change branching priority of variable in "
                   "non-MIP problem!";
  }
}

void KnitroInterface::AddRowConstraint(MPConstraint* ct) {
  sync_status_ = MUST_RELOAD;
}

void KnitroInterface::AddVariable(MPVariable* var) {
  sync_status_ = MUST_RELOAD;
}

void KnitroInterface::ExtractNewVariables() {
  int const total_num_vars = solver_->variables_.size();
  if (total_num_vars > last_variable_index_) {
    int const len = total_num_vars - last_variable_index_;
    // for init and def basic properties
    std::unique_ptr<int[]> idx_vars(new int[len]);
    std::unique_ptr<double[]> lb(new double[len]);
    std::unique_ptr<double[]> ub(new double[len]);
    std::unique_ptr<int[]> types(new int[len]);
    // lambda fn to destroy the array of names
    auto deleter = [len](char** ptr) {
      for (int i = 0; i < len; ++i) {
        delete[] ptr[i];
      }
      delete[] ptr;
    };
    std::unique_ptr<char*[], decltype(deleter)> names(new char*[len], deleter);
    // for priority properties
    std::unique_ptr<int[]> prior(new int[len]);
    std::unique_ptr<int[]> prior_idx(new int[len]);
    int prior_nb = 0;

    // Define new variables
    for (int j = 0, var_index = last_variable_index_; j < len;
         ++j, ++var_index) {
      MPVariable* const var = solver_->variables_[var_index];
      DCHECK(!variable_is_extracted(var_index));
      set_variable_as_extracted(var_index, true);
      idx_vars[j] = var_index;
      lb[j] = redefine_infinity_double(var->lb());
      ub[j] = redefine_infinity_double(var->ub());
      // Def buffer size at 256 for variables' name
      names[j] = new char[256];
      strcpy(names[j], var->name().c_str());
      types[j] =
          (mip_ && var->integer()) ? KN_VARTYPE_INTEGER : KN_VARTYPE_CONTINUOUS;
      if (var->integer() && (var->branching_priority() != 0)) {
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
    CHECK_STATUS(KN_set_mip_branching_priorities(kc_, prior_nb, prior_idx.get(),
                                                 prior.get()));

    // Add new variables to existing constraints.
    for (int i = 0; i < last_constraint_index_; i++) {
      MPConstraint* const ct = solver_->constraints_[i];
      for (const auto& entry : ct->coefficients_) {
        const int var_index = entry.first->index();
        DCHECK(variable_is_extracted(var_index));
        if (var_index >= last_variable_index_) {
          // The variable is new, so we know the previous coefficient
          // value was 0 and we can directly add the coefficient.
          CHECK_STATUS(KN_add_con_linear_term(kc_, i, var_index, entry.second));
        }
      }
    }
  }
}

void KnitroInterface::ExtractNewConstraints() {
  int const total_num_cons = solver_->constraints_.size();
  int const total_num_vars = solver_->variables_.size();
  if (total_num_cons > last_constraint_index_) {
    int const len = total_num_cons - last_constraint_index_;
    std::unique_ptr<int[]> idx_cons(new int[len]);
    std::unique_ptr<double[]> lb(new double[len]);
    std::unique_ptr<double[]> ub(new double[len]);
    std::unique_ptr<int[]> lin_idx_cons(new int[len * total_num_vars]);
    std::unique_ptr<int[]> lin_idx_vars(new int[len * total_num_vars]);
    std::unique_ptr<double[]> lin_coefs(new double[len * total_num_vars]);
    // lambda fn to destroy the array of names
    auto deleter = [len](char** ptr) {
      for (int i = 0; i < len; ++i) {
        delete[] ptr[i];
      }
      delete[] ptr;
    };
    std::unique_ptr<char*[], decltype(deleter)> names(new char*[len], deleter);
    int idx_lin_term = 0;
    // Define new constraints
    for (int j = 0, con_index = last_constraint_index_; j < len;
         ++j, ++con_index) {
      MPConstraint* const ct = solver_->constraints_[con_index];
      DCHECK(!constraint_is_extracted(con_index));
      set_constraint_as_extracted(con_index, true);
      idx_cons[j] = con_index;
      lb[j] = redefine_infinity_double(ct->lb());
      ub[j] = redefine_infinity_double(ct->ub());
      for (int i = 0; i < total_num_vars; ++i) {
        lin_idx_cons[idx_lin_term] = con_index;
        lin_idx_vars[idx_lin_term] = i;
        lin_coefs[idx_lin_term] = ct->GetCoefficient(solver_->variables_[i]);
        idx_lin_term++;
      }
      // Def buffer size at 256 for variables' name
      names[j] = new char[256];
      strcpy(names[j], ct->name().c_str());
    }
    CHECK_STATUS(KN_add_cons(kc_, len, NULL));
    CHECK_STATUS(KN_set_con_lobnds(kc_, len, idx_cons.get(), lb.get()));
    CHECK_STATUS(KN_set_con_upbnds(kc_, len, idx_cons.get(), ub.get()));

    CHECK_STATUS(KN_set_con_names(kc_, len, idx_cons.get(), names.get()));
    if (idx_lin_term) {
      CHECK_STATUS(
          KN_add_con_linear_struct(kc_, idx_lin_term, lin_idx_cons.get(),
                                   lin_idx_vars.get(), lin_coefs.get()));
      KN_update(kc_);
    }
  }
}

void KnitroInterface::ExtractObjective() {
  int const len = solver_->variables_.size();

  if (len) {
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
    // if a init solve occured, remove prev coef to add the new ones
    if (!no_obj_) {
      CHECK_STATUS(KN_chg_obj_linear_struct(kc_, len, ind.get(), val.get()));
      CHECK_STATUS(KN_chg_obj_constant(kc_, solver_->Objective().offset()));
    } else {
      CHECK_STATUS(KN_add_obj_linear_struct(kc_, len, ind.get(), val.get()));
      CHECK_STATUS(KN_add_obj_constant(kc_, solver_->Objective().offset()));
    }

    CHECK_STATUS(KN_update(kc_));
    no_obj_ = false;
  }

  // Extra check on the optimization direction
  SetOptimizationDirection(maximize_);
}

// ------ Parameters  -----

void KnitroInterface::SetParameters(const MPSolverParameters& param) {
  SetCommonParameters(param);
  SetScalingMode(param.GetIntegerParam(MPSolverParameters::SCALING));
  if (mip_) SetMIPParameters(param);
}

bool KnitroInterface::SetSolverSpecificParametersAsString(
    const std::string& parameters) {
  if (parameters.empty()) {
    return true;
  }
  if (KN_load_param_file(kc_, parameters.c_str()) == 0) {
    return true;
  }
  return false;
}

void KnitroInterface::SetRelativeMipGap(double value) {
  /**
   * This method should be called by SetMIPParameters() only
   * so there is no mip_ check here
   */
  CHECK_STATUS(KN_set_double_param(kc_, KN_PARAM_MIP_OPTGAPREL, value));
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
    default:
      SetIntegerParamToUnsupportedValue(MPSolverParameters::PRESOLVE, value);
      return;
  }
}

void KnitroInterface::SetScalingMode(int value) {
  auto const scaling = static_cast<MPSolverParameters::ScalingValues>(value);

  switch (scaling) {
    case MPSolverParameters::SCALING_OFF:
      CHECK_STATUS(KN_set_int_param(kc_, KN_PARAM_LINSOLVER_SCALING,
                                    KN_LINSOLVER_SCALING_NONE));
      break;
    case MPSolverParameters::SCALING_ON:
      CHECK_STATUS(KN_set_int_param(kc_, KN_PARAM_LINSOLVER_SCALING,
                                    KN_LINSOLVER_SCALING_ALWAYS));
      break;
  }
}

void KnitroInterface::SetLpAlgorithm(int value) {
  auto const algorithm =
      static_cast<MPSolverParameters::LpAlgorithmValues>(value);
  switch (algorithm) {
    case MPSolverParameters::PRIMAL:
      CHECK_STATUS(
          KN_set_int_param(kc_, KN_PARAM_ACT_LPALG, KN_ACT_LPALG_PRIMAL));
      break;
    case MPSolverParameters::DUAL:
      CHECK_STATUS(
          KN_set_int_param(kc_, KN_PARAM_ACT_LPALG, KN_ACT_LPALG_DUAL));
      break;
    case MPSolverParameters::BARRIER:
      CHECK_STATUS(
          KN_set_int_param(kc_, KN_PARAM_ACT_LPALG, KN_ACT_LPALG_BARRIER));
      break;
    default:
      CHECK_STATUS(
          KN_set_int_param(kc_, KN_PARAM_ACT_LPALG, KN_ACT_LPALG_DEFAULT));
      break;
  }
}

void KnitroInterface::SetCallback(MPCallback* mp_callback) {
  callback_ = mp_callback;
}

absl::Status KnitroInterface::SetNumThreads(int num_threads) {
  CHECK_STATUS(KN_set_int_param(kc_, KN_PARAM_NUMTHREADS, num_threads));
  return absl::OkStatus();
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

  if (quiet_) {
    // Silent the screen output
    CHECK_STATUS(KN_set_int_param(kc_, KN_PARAM_OUTLEV, KN_OUTLEV_NONE));
  }

  // Set parameters.
  SetParameters(param);
  solver_->SetSolverSpecificParametersAsString(
      solver_->solver_specific_parameter_string_);
  if (solver_->time_limit()) {
    VLOG(1) << "Setting time limit = " << solver_->time_limit() << " ms.";
    CHECK_STATUS(KN_set_double_param(kc_, KN_PARAM_MAXTIMECPU,
                                     solver_->time_limit_in_secs()));
  }

  // Set the hint (if any)
  this->AddSolutionHintToOptimizer();

  if (callback_ != nullptr) {
    if (callback_->might_add_lazy_constraints()) {
      MPCallBackWithEvent cbe;
      cbe.callback = callback_;
      cbe.event = MPCallbackEvent::kMipSolution;
      CHECK_STATUS(KN_set_mip_lazyconstraints_callback(
          kc_, CallBackFn, static_cast<void*>(&cbe)));
    }
    if (callback_->might_add_cuts()) {
      MPCallBackWithEvent cbe;
      cbe.callback = callback_;
      cbe.event = MPCallbackEvent::kMipNode;
      CHECK_STATUS(KN_set_mip_usercuts_callback(kc_, CallBackFn,
                                                static_cast<void*>(&cbe)));
    }
  }

  // Special case for empty model (no var)
  // Infeasible Constraint should have been checked
  // by MPSolver upstream
  if (!solver_->NumVariables()) {
    objective_value_ = solver_->Objective().offset();
    if (mip_) best_objective_bound_ = 0;
    result_status_ = MPSolver::OPTIMAL;
    sync_status_ = SOLUTION_SYNCHRONIZED;
    return result_status_;
  }

  // Solve.
  timer.Restart();
  int status;
  status = -KN_solve(kc_);

  if (status == 0) {
    // the final solution is optimal to specified tolerances;
    result_status_ = MPSolver::OPTIMAL;

  } else if ((status < 110 && 100 <= status) ||
             (status < 410 && 400 <= status)) {
    // a feasible solution was found (but not verified optimal)
    // OR
    // a feasible point was found before reaching the limit
    result_status_ = MPSolver::FEASIBLE;

  } else if ((status < 210 && 200 <= status) ||
             (status < 420 && 410 <= status)) {
    // Knitro terminated at an infeasible point;
    // OR
    // no feasible point was found before reaching the limit
    result_status_ = MPSolver::INFEASIBLE;

  } else if (status < 302 && 300 <= status) {
    // the problem was determined to be unbounded;
    result_status_ = MPSolver::UNBOUNDED;

  } else {
    // Knitro terminated with an input error or some non-standard error or else.
    result_status_ = MPSolver::ABNORMAL;
  }

  if (result_status_ == MPSolver::OPTIMAL ||
      result_status_ == MPSolver::FEASIBLE) {
    // If optimal or feasible solution is found.
    SetSolution();
  } else {
    VLOG(1) << "No feasible solution found.";
  }

  sync_status_ = SOLUTION_SYNCHRONIZED;
  return result_status_;
}

void KnitroInterface::SetSolution() {
  int status;
  int const nb_vars = solver_->variables_.size();
  int const nb_cons = solver_->constraints_.size();
  if (nb_vars) {
    std::unique_ptr<double[]> values(new double[nb_vars]);
    std::unique_ptr<double[]> reduced_costs(new double[nb_vars]);
    CHECK_STATUS(
        KN_get_solution(kc_, &status, &objective_value_, values.get(), NULL));
    CHECK_STATUS(KN_get_var_dual_values_all(kc_, reduced_costs.get()));
    for (int j = 0; j < nb_vars; ++j) {
      MPVariable* var = solver_->variables_[j];
      var->set_solution_value(values[j]);
      if (!mip_) var->set_reduced_cost(-reduced_costs[j]);
    }
  }
  if (nb_cons) {
    std::unique_ptr<double[]> duals_cons(new double[nb_cons]);
    CHECK_STATUS(KN_get_con_dual_values_all(kc_, duals_cons.get()));
    if (!mip_) {
      for (int j = 0; j < nb_cons; ++j) {
        MPConstraint* ct = solver_->constraints_[j];
        ct->set_dual_value(-duals_cons[j]);
      }
    }
  }
  if (mip_) {
    double rel_gap;
    CHECK_STATUS(KN_get_mip_rel_gap(kc_, &rel_gap));
    best_objective_bound_ = objective_value_ + rel_gap;
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
  CHECK_STATUS(
      KN_set_var_primal_init_values(kc_, len, col_ind.get(), val.get()));
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
  int const length = 15;     // should contain the string termination character
  char release[length + 1];  // checked but there are trouble if not allocated
                             // with a additional char

  CHECK_STATUS(KN_get_release(length, release));

  return absl::StrFormat("Knitro library version %s", release);
}

MPSolverInterface* BuildKnitroInterface(bool mip, MPSolver* const solver) {
  return new KnitroInterface(solver, mip);
}

}  // namespace operations_research