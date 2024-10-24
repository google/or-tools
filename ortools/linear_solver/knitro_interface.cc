// ADD HEADER

#include <algorithm>
#include <clocale>
#include <cmath>
#include <fstream>
#include <iostream>
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

static std::map<std::string, int>& getMapParam() {
  static std::map<std::string, int> mapControls = {
      {"KN_PARAM_NEWPOINT", KN_PARAM_NEWPOINT},
      {"KN_PARAM_HONORBNDS", KN_PARAM_HONORBNDS},
      {"KN_PARAM_ALGORITHM", KN_PARAM_ALGORITHM},
      {"KN_PARAM_ALG", KN_PARAM_ALG},
      {"KN_PARAM_BAR_MURULE", KN_PARAM_BAR_MURULE},
      {"KN_PARAM_BAR_FEASIBLE", KN_PARAM_BAR_FEASIBLE},
      {"KN_PARAM_GRADOPT", KN_PARAM_GRADOPT},
      {"KN_PARAM_HESSOPT", KN_PARAM_HESSOPT},
      {"KN_PARAM_BAR_INITPT", KN_PARAM_BAR_INITPT},
      {"KN_PARAM_ACT_LPSOLVER", KN_PARAM_ACT_LPSOLVER},
      {"KN_PARAM_CG_MAXIT", KN_PARAM_CG_MAXIT},
      {"KN_PARAM_MAXIT", KN_PARAM_MAXIT},
      {"KN_PARAM_OUTLEV", KN_PARAM_OUTLEV},
      {"KN_PARAM_OUTMODE", KN_PARAM_OUTMODE},
      {"KN_PARAM_SCALE", KN_PARAM_SCALE},
      {"KN_PARAM_SOC", KN_PARAM_SOC},
      {"KN_PARAM_DELTA", KN_PARAM_DELTA},
      {"KN_PARAM_BAR_FEASMODETOL", KN_PARAM_BAR_FEASMODETOL},
      {"KN_PARAM_FEASTOL", KN_PARAM_FEASTOL},
      {"KN_PARAM_FEASTOLABS", KN_PARAM_FEASTOLABS},
      {"KN_PARAM_MAXTIMECPU", KN_PARAM_MAXTIMECPU},
      {"KN_PARAM_BAR_INITMU", KN_PARAM_BAR_INITMU},
      {"KN_PARAM_OBJRANGE", KN_PARAM_OBJRANGE},
      {"KN_PARAM_OPTTOL", KN_PARAM_OPTTOL},
      {"KN_PARAM_OPTTOLABS", KN_PARAM_OPTTOLABS},
      {"KN_PARAM_LINSOLVER_PIVOTTOL", KN_PARAM_LINSOLVER_PIVOTTOL},
      {"KN_PARAM_XTOL", KN_PARAM_XTOL},
      {"KN_PARAM_DEBUG", KN_PARAM_DEBUG},
      {"KN_PARAM_MULTISTART", KN_PARAM_MULTISTART},
      {"KN_PARAM_MSENABLE", KN_PARAM_MSENABLE},
      {"KN_PARAM_MS_ENABLE", KN_PARAM_MS_ENABLE},
      {"KN_PARAM_MSMAXSOLVES", KN_PARAM_MSMAXSOLVES},
      {"KN_PARAM_MS_MAXSOLVES", KN_PARAM_MS_MAXSOLVES},
      {"KN_PARAM_MSMAXBNDRANGE", KN_PARAM_MSMAXBNDRANGE},
      {"KN_PARAM_MS_MAXBNDRANGE", KN_PARAM_MS_MAXBNDRANGE},
      {"KN_PARAM_MSMAXTIMECPU", KN_PARAM_MSMAXTIMECPU},
      {"KN_PARAM_MS_MAXTIMECPU", KN_PARAM_MS_MAXTIMECPU},
      {"KN_PARAM_MSMAXTIMEREAL", KN_PARAM_MSMAXTIMEREAL},
      {"KN_PARAM_MS_MAXTIMEREAL", KN_PARAM_MS_MAXTIMEREAL},
      {"KN_PARAM_LMSIZE", KN_PARAM_LMSIZE},
      {"KN_PARAM_BAR_MAXCROSSIT", KN_PARAM_BAR_MAXCROSSIT},
      {"KN_PARAM_MAXTIMEREAL", KN_PARAM_MAXTIMEREAL},
      {"KN_PARAM_CG_PRECOND", KN_PARAM_CG_PRECOND},
      {"KN_PARAM_BLASOPTION", KN_PARAM_BLASOPTION},
      {"KN_PARAM_BAR_MAXREFACTOR", KN_PARAM_BAR_MAXREFACTOR},
      {"KN_PARAM_LINESEARCH_MAXTRIALS", KN_PARAM_LINESEARCH_MAXTRIALS},
      {"KN_PARAM_BLASOPTIONLIB", KN_PARAM_BLASOPTIONLIB},
      {"KN_PARAM_OUTAPPEND", KN_PARAM_OUTAPPEND},
      {"KN_PARAM_OUTDIR", KN_PARAM_OUTDIR},
      {"KN_PARAM_CPLEXLIB", KN_PARAM_CPLEXLIB},
      {"KN_PARAM_BAR_PENRULE", KN_PARAM_BAR_PENRULE},
      {"KN_PARAM_BAR_PENCONS", KN_PARAM_BAR_PENCONS},
      {"KN_PARAM_MSNUMTOSAVE", KN_PARAM_MSNUMTOSAVE},
      {"KN_PARAM_MS_NUMTOSAVE", KN_PARAM_MS_NUMTOSAVE},
      {"KN_PARAM_MSSAVETOL", KN_PARAM_MSSAVETOL},
      {"KN_PARAM_MS_SAVETOL", KN_PARAM_MS_SAVETOL},
      {"KN_PARAM_PRESOLVEDEBUG", KN_PARAM_PRESOLVEDEBUG},
      {"KN_PARAM_MSTERMINATE", KN_PARAM_MSTERMINATE},
      {"KN_PARAM_MS_TERMINATE", KN_PARAM_MS_TERMINATE},
      {"KN_PARAM_MSSTARTPTRANGE", KN_PARAM_MSSTARTPTRANGE},
      {"KN_PARAM_MS_STARTPTRANGE", KN_PARAM_MS_STARTPTRANGE},
      {"KN_PARAM_INFEASTOL", KN_PARAM_INFEASTOL},
      {"KN_PARAM_LINSOLVER", KN_PARAM_LINSOLVER},
      {"KN_PARAM_BAR_DIRECTINTERVAL", KN_PARAM_BAR_DIRECTINTERVAL},
      {"KN_PARAM_PRESOLVE", KN_PARAM_PRESOLVE},
      {"KN_PARAM_PRESOLVE_TOL", KN_PARAM_PRESOLVE_TOL},
      {"KN_PARAM_BAR_SWITCHRULE", KN_PARAM_BAR_SWITCHRULE},
      {"KN_PARAM_HESSIAN_NO_F", KN_PARAM_HESSIAN_NO_F},
      {"KN_PARAM_MA_TERMINATE", KN_PARAM_MA_TERMINATE},
      {"KN_PARAM_MA_MAXTIMECPU", KN_PARAM_MA_MAXTIMECPU},
      {"KN_PARAM_MA_MAXTIMEREAL", KN_PARAM_MA_MAXTIMEREAL},
      {"KN_PARAM_MSSEED", KN_PARAM_MSSEED},
      {"KN_PARAM_MS_SEED", KN_PARAM_MS_SEED},
      {"KN_PARAM_MA_OUTSUB", KN_PARAM_MA_OUTSUB},
      {"KN_PARAM_MS_OUTSUB", KN_PARAM_MS_OUTSUB},
      {"KN_PARAM_XPRESSLIB", KN_PARAM_XPRESSLIB},
      {"KN_PARAM_TUNER", KN_PARAM_TUNER},
      {"KN_PARAM_TUNER_OPTIONSFILE", KN_PARAM_TUNER_OPTIONSFILE},
      {"KN_PARAM_TUNER_MAXTIMECPU", KN_PARAM_TUNER_MAXTIMECPU},
      {"KN_PARAM_TUNER_MAXTIMEREAL", KN_PARAM_TUNER_MAXTIMEREAL},
      {"KN_PARAM_TUNER_OUTSUB", KN_PARAM_TUNER_OUTSUB},
      {"KN_PARAM_TUNER_TERMINATE", KN_PARAM_TUNER_TERMINATE},
      {"KN_PARAM_LINSOLVER_OOC", KN_PARAM_LINSOLVER_OOC},
      {"KN_PARAM_BAR_RELAXCONS", KN_PARAM_BAR_RELAXCONS},
      {"KN_PARAM_MSDETERMINISTIC", KN_PARAM_MSDETERMINISTIC},
      {"KN_PARAM_MS_DETERMINISTIC", KN_PARAM_MS_DETERMINISTIC},
      {"KN_PARAM_BAR_REFINEMENT", KN_PARAM_BAR_REFINEMENT},
      {"KN_PARAM_DERIVCHECK", KN_PARAM_DERIVCHECK},
      {"KN_PARAM_DERIVCHECK_TYPE", KN_PARAM_DERIVCHECK_TYPE},
      {"KN_PARAM_DERIVCHECK_TOL", KN_PARAM_DERIVCHECK_TOL},
      {"KN_PARAM_LINSOLVER_INEXACT", KN_PARAM_LINSOLVER_INEXACT},
      {"KN_PARAM_LINSOLVER_INEXACTTOL", KN_PARAM_LINSOLVER_INEXACTTOL},
      {"KN_PARAM_MAXFEVALS", KN_PARAM_MAXFEVALS},
      {"KN_PARAM_FSTOPVAL", KN_PARAM_FSTOPVAL},
      {"KN_PARAM_DATACHECK", KN_PARAM_DATACHECK},
      {"KN_PARAM_DERIVCHECK_TERMINATE", KN_PARAM_DERIVCHECK_TERMINATE},
      {"KN_PARAM_BAR_WATCHDOG", KN_PARAM_BAR_WATCHDOG},
      {"KN_PARAM_FTOL", KN_PARAM_FTOL},
      {"KN_PARAM_FTOL_ITERS", KN_PARAM_FTOL_ITERS},
      {"KN_PARAM_ACT_QPALG", KN_PARAM_ACT_QPALG},
      {"KN_PARAM_BAR_INITPI_MPEC", KN_PARAM_BAR_INITPI_MPEC},
      {"KN_PARAM_XTOL_ITERS", KN_PARAM_XTOL_ITERS},
      {"KN_PARAM_LINESEARCH", KN_PARAM_LINESEARCH},
      {"KN_PARAM_OUT_CSVINFO", KN_PARAM_OUT_CSVINFO},
      {"KN_PARAM_INITPENALTY", KN_PARAM_INITPENALTY},
      {"KN_PARAM_ACT_LPFEASTOL", KN_PARAM_ACT_LPFEASTOL},
      {"KN_PARAM_CG_STOPTOL", KN_PARAM_CG_STOPTOL},
      {"KN_PARAM_RESTARTS", KN_PARAM_RESTARTS},
      {"KN_PARAM_RESTARTS_MAXIT", KN_PARAM_RESTARTS_MAXIT},
      {"KN_PARAM_BAR_SLACKBOUNDPUSH", KN_PARAM_BAR_SLACKBOUNDPUSH},
      {"KN_PARAM_CG_PMEM", KN_PARAM_CG_PMEM},
      {"KN_PARAM_BAR_SWITCHOBJ", KN_PARAM_BAR_SWITCHOBJ},
      {"KN_PARAM_OUTNAME", KN_PARAM_OUTNAME},
      {"KN_PARAM_OUT_CSVNAME", KN_PARAM_OUT_CSVNAME},
      {"KN_PARAM_ACT_PARAMETRIC", KN_PARAM_ACT_PARAMETRIC},
      {"KN_PARAM_ACT_LPDUMPMPS", KN_PARAM_ACT_LPDUMPMPS},
      {"KN_PARAM_ACT_LPALG", KN_PARAM_ACT_LPALG},
      {"KN_PARAM_ACT_LPPRESOLVE", KN_PARAM_ACT_LPPRESOLVE},
      {"KN_PARAM_ACT_LPPENALTY", KN_PARAM_ACT_LPPENALTY},
      {"KN_PARAM_BNDRANGE", KN_PARAM_BNDRANGE},
      {"KN_PARAM_BAR_CONIC_ENABLE", KN_PARAM_BAR_CONIC_ENABLE},
      {"KN_PARAM_CONVEX", KN_PARAM_CONVEX},
      {"KN_PARAM_OUT_HINTS", KN_PARAM_OUT_HINTS},
      {"KN_PARAM_EVAL_FCGA", KN_PARAM_EVAL_FCGA},
      {"KN_PARAM_BAR_MAXCORRECTORS", KN_PARAM_BAR_MAXCORRECTORS},
      {"KN_PARAM_STRAT_WARM_START", KN_PARAM_STRAT_WARM_START},
      {"KN_PARAM_FINDIFF_TERMINATE", KN_PARAM_FINDIFF_TERMINATE},
      {"KN_PARAM_CPUPLATFORM", KN_PARAM_CPUPLATFORM},
      {"KN_PARAM_PRESOLVE_PASSES", KN_PARAM_PRESOLVE_PASSES},
      {"KN_PARAM_PRESOLVE_LEVEL", KN_PARAM_PRESOLVE_LEVEL},
      {"KN_PARAM_FINDIFF_RELSTEPSIZE", KN_PARAM_FINDIFF_RELSTEPSIZE},
      {"KN_PARAM_INFEASTOL_ITERS", KN_PARAM_INFEASTOL_ITERS},
      {"KN_PARAM_PRESOLVEOP_TIGHTEN", KN_PARAM_PRESOLVEOP_TIGHTEN},
      {"KN_PARAM_BAR_LINSYS", KN_PARAM_BAR_LINSYS},
      {"KN_PARAM_PRESOLVE_INITPT", KN_PARAM_PRESOLVE_INITPT},
      {"KN_PARAM_ACT_QPPENALTY", KN_PARAM_ACT_QPPENALTY},
      {"KN_PARAM_BAR_LINSYS_STORAGE", KN_PARAM_BAR_LINSYS_STORAGE},
      {"KN_PARAM_LINSOLVER_MAXITREF", KN_PARAM_LINSOLVER_MAXITREF},
      {"KN_PARAM_BFGS_SCALING", KN_PARAM_BFGS_SCALING},
      {"KN_PARAM_BAR_INITSHIFTTOL", KN_PARAM_BAR_INITSHIFTTOL},
      {"KN_PARAM_NUMTHREADS", KN_PARAM_NUMTHREADS},
      {"KN_PARAM_CONCURRENT_EVALS", KN_PARAM_CONCURRENT_EVALS},
      {"KN_PARAM_BLAS_NUMTHREADS", KN_PARAM_BLAS_NUMTHREADS},
      {"KN_PARAM_LINSOLVER_NUMTHREADS", KN_PARAM_LINSOLVER_NUMTHREADS},
      {"KN_PARAM_MS_NUMTHREADS", KN_PARAM_MS_NUMTHREADS},
      {"KN_PARAM_CONIC_NUMTHREADS", KN_PARAM_CONIC_NUMTHREADS},
      {"KN_PARAM_NCVX_QCQP_INIT", KN_PARAM_NCVX_QCQP_INIT},
      {"KN_PARAM_FINDIFF_ESTNOISE", KN_PARAM_FINDIFF_ESTNOISE},
      {"KN_PARAM_FINDIFF_NUMTHREADS", KN_PARAM_FINDIFF_NUMTHREADS},
      {"KN_PARAM_BAR_MPEC_HEURISTIC", KN_PARAM_BAR_MPEC_HEURISTIC},
      {"KN_PARAM_PRESOLVEOP_REDUNDANT", KN_PARAM_PRESOLVEOP_REDUNDANT},
      {"KN_PARAM_LINSOLVER_ORDERING", KN_PARAM_LINSOLVER_ORDERING},
      {"KN_PARAM_LINSOLVER_NODEAMALG", KN_PARAM_LINSOLVER_NODEAMALG},
      {"KN_PARAM_PRESOLVEOP_SUBSTITUTION", KN_PARAM_PRESOLVEOP_SUBSTITUTION},
      {"KN_PARAM_PRESOLVEOP_SUBSTITUTION_TOL",
       KN_PARAM_PRESOLVEOP_SUBSTITUTION_TOL},
      {"KN_PARAM_MS_INITPT_CLUSTER", KN_PARAM_MS_INITPT_CLUSTER},
      {"KN_PARAM_SCALE_VARS", KN_PARAM_SCALE_VARS},
      {"KN_PARAM_BAR_MAXMU", KN_PARAM_BAR_MAXMU},
      {"KN_PARAM_BAR_GLOBALIZE", KN_PARAM_BAR_GLOBALIZE},
      {"KN_PARAM_LINSOLVER_SCALING", KN_PARAM_LINSOLVER_SCALING},
      {"KN_PARAM_MIP_METHOD", KN_PARAM_MIP_METHOD},
      {"KN_PARAM_MIP_BRANCHRULE", KN_PARAM_MIP_BRANCHRULE},
      {"KN_PARAM_MIP_SELECTRULE", KN_PARAM_MIP_SELECTRULE},
      {"KN_PARAM_MIP_INTGAPABS", KN_PARAM_MIP_INTGAPABS},
      {"KN_PARAM_MIP_OPTGAPABS", KN_PARAM_MIP_OPTGAPABS},
      {"KN_PARAM_MIP_INTGAPREL", KN_PARAM_MIP_INTGAPREL},
      {"KN_PARAM_MIP_OPTGAPREL", KN_PARAM_MIP_OPTGAPREL},
      {"KN_PARAM_MIP_MAXTIMECPU", KN_PARAM_MIP_MAXTIMECPU},
      {"KN_PARAM_MIP_MAXTIMEREAL", KN_PARAM_MIP_MAXTIMEREAL},
      {"KN_PARAM_MIP_MAXSOLVES", KN_PARAM_MIP_MAXSOLVES},
      {"KN_PARAM_MIP_INTEGERTOL", KN_PARAM_MIP_INTEGERTOL},
      {"KN_PARAM_MIP_OUTLEVEL", KN_PARAM_MIP_OUTLEVEL},
      {"KN_PARAM_MIP_OUTINTERVAL", KN_PARAM_MIP_OUTINTERVAL},
      {"KN_PARAM_MIP_OUTSUB", KN_PARAM_MIP_OUTSUB},
      {"KN_PARAM_MIP_DEBUG", KN_PARAM_MIP_DEBUG},
      {"KN_PARAM_MIP_IMPLICATNS", KN_PARAM_MIP_IMPLICATNS},
      {"KN_PARAM_MIP_IMPLICATIONS", KN_PARAM_MIP_IMPLICATIONS},
      {"KN_PARAM_MIP_GUB_BRANCH", KN_PARAM_MIP_GUB_BRANCH},
      {"KN_PARAM_MIP_KNAPSACK", KN_PARAM_MIP_KNAPSACK},
      {"KN_PARAM_MIP_ROUNDING", KN_PARAM_MIP_ROUNDING},
      {"KN_PARAM_MIP_ROOTALG", KN_PARAM_MIP_ROOTALG},
      {"KN_PARAM_MIP_LPALG", KN_PARAM_MIP_LPALG},
      {"KN_PARAM_MIP_TERMINATE", KN_PARAM_MIP_TERMINATE},
      {"KN_PARAM_MIP_MAXNODES", KN_PARAM_MIP_MAXNODES},
      {"KN_PARAM_MIP_HEURISTIC", KN_PARAM_MIP_HEURISTIC},
      {"KN_PARAM_MIP_HEUR_MAXIT", KN_PARAM_MIP_HEUR_MAXIT},
      {"KN_PARAM_MIP_HEUR_MAXTIMECPU", KN_PARAM_MIP_HEUR_MAXTIMECPU},
      {"KN_PARAM_MIP_HEUR_MAXTIMEREAL", KN_PARAM_MIP_HEUR_MAXTIMEREAL},
      {"KN_PARAM_MIP_PSEUDOINIT", KN_PARAM_MIP_PSEUDOINIT},
      {"KN_PARAM_MIP_STRONG_MAXIT", KN_PARAM_MIP_STRONG_MAXIT},
      {"KN_PARAM_MIP_STRONG_CANDLIM", KN_PARAM_MIP_STRONG_CANDLIM},
      {"KN_PARAM_MIP_STRONG_LEVEL", KN_PARAM_MIP_STRONG_LEVEL},
      {"KN_PARAM_MIP_INTVAR_STRATEGY", KN_PARAM_MIP_INTVAR_STRATEGY},
      {"KN_PARAM_MIP_RELAXABLE", KN_PARAM_MIP_RELAXABLE},
      {"KN_PARAM_MIP_NODEALG", KN_PARAM_MIP_NODEALG},
      {"KN_PARAM_MIP_HEUR_TERMINATE", KN_PARAM_MIP_HEUR_TERMINATE},
      {"KN_PARAM_MIP_SELECTDIR", KN_PARAM_MIP_SELECTDIR},
      {"KN_PARAM_MIP_CUTFACTOR", KN_PARAM_MIP_CUTFACTOR},
      {"KN_PARAM_MIP_ZEROHALF", KN_PARAM_MIP_ZEROHALF},
      {"KN_PARAM_MIP_MIR", KN_PARAM_MIP_MIR},
      {"KN_PARAM_MIP_CLIQUE", KN_PARAM_MIP_CLIQUE},
      {"KN_PARAM_MIP_HEUR_STRATEGY", KN_PARAM_MIP_HEUR_STRATEGY},
      {"KN_PARAM_MIP_HEUR_FEASPUMP", KN_PARAM_MIP_HEUR_FEASPUMP},
      {"KN_PARAM_MIP_HEUR_MPEC", KN_PARAM_MIP_HEUR_MPEC},
      {"KN_PARAM_MIP_HEUR_DIVING", KN_PARAM_MIP_HEUR_DIVING},
      {"KN_PARAM_MIP_CUTTINGPLANE", KN_PARAM_MIP_CUTTINGPLANE},
      {"KN_PARAM_MIP_CUTOFF", KN_PARAM_MIP_CUTOFF},
      {"KN_PARAM_MIP_HEUR_LNS", KN_PARAM_MIP_HEUR_LNS},
      {"KN_PARAM_MIP_MULTISTART", KN_PARAM_MIP_MULTISTART},
      {"KN_PARAM_MIP_LIFTPROJECT", KN_PARAM_MIP_LIFTPROJECT},
      {"KN_PARAM_MIP_NUMTHREADS", KN_PARAM_MIP_NUMTHREADS},
      {"KN_PARAM_MIP_HEUR_MISQP", KN_PARAM_MIP_HEUR_MISQP},
      {"KN_PARAM_MIP_RESTART", KN_PARAM_MIP_RESTART},
      {"KN_PARAM_MIP_GOMORY", KN_PARAM_MIP_GOMORY},
      {"KN_PARAM_MIP_CUT_PROBING", KN_PARAM_MIP_CUT_PROBING},
      {"KN_PARAM_MIP_CUT_FLOWCOVER", KN_PARAM_MIP_CUT_FLOWCOVER},
      {"KN_PARAM_MIP_HEUR_LOCALSEARCH", KN_PARAM_MIP_HEUR_LOCALSEARCH},
      {"KN_PARAM_PAR_NUMTHREADS", KN_PARAM_PAR_NUMTHREADS},
      {"KN_PARAM_PAR_CONCURRENT_EVALS", KN_PARAM_PAR_CONCURRENT_EVALS},
      {"KN_PARAM_PAR_BLASNUMTHREADS", KN_PARAM_PAR_BLASNUMTHREADS},
      {"KN_PARAM_PAR_LSNUMTHREADS", KN_PARAM_PAR_LSNUMTHREADS},
      {"KN_PARAM_PAR_MSNUMTHREADS", KN_PARAM_PAR_MSNUMTHREADS},
      {"KN_PARAM_PAR_CONICNUMTHREADS", KN_PARAM_PAR_CONICNUMTHREADS},
  };
  return mapControls;
}

/*------------KnitroInterface Definition------------*/

class KnitroInterface : public MPSolverInterface {
 public:
  explicit KnitroInterface(MPSolver* solver, bool mip);
  ~KnitroInterface() override;

  MPSolver::ResultStatus Solve(const MPSolverParameters& param) override;

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
  std::map<std::string, int> param_map_;
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
  const int num_terms = linear_range.linear_expr().terms().size();
  std::unique_ptr<int[]> var_indexes(new int[num_terms]);
  std::unique_ptr<double[]> var_coefficients(new double[num_terms]);
  int term_index = 0;
  for (const auto& var_coef_pair : linear_range.linear_expr().terms()) {
    var_indexes[term_index] = var_coef_pair.first->index();
    var_coefficients[term_index] = var_coef_pair.second;
    ++term_index;
  }
  int cb_con;
  CHECK_STATUS(KN_add_con(kc, &cb_con));
  CHECK_STATUS(KN_set_con_lobnd(
      kc, cb_con, redefine_infinity_double(linear_range.lower_bound())));
  CHECK_STATUS(KN_set_con_upbnd(
      kc, cb_con, redefine_infinity_double(linear_range.upper_bound())));
  CHECK_STATUS(KN_add_con_linear_struct_one(kc, num_terms, cb_con,
                                            var_indexes.get(),
                                            var_coefficients.get()));
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
    : MPSolverInterface(solver),
      kc_(nullptr),
      mip_(mip),
      no_obj_(true),
      param_map_(getMapParam()) {
  KnitroIsCorrectlyInstalled();
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
    // if the variable has been extracted then its linear coefficient exists
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
    int const number_added_vars = total_num_vars - last_variable_index_;

    std::unique_ptr<int[]> idx_vars(new int[number_added_vars]);
    std::unique_ptr<double[]> lb(new double[number_added_vars]);
    std::unique_ptr<double[]> ub(new double[number_added_vars]);
    std::unique_ptr<int[]> types(new int[number_added_vars]);
    // for priority properties
    std::unique_ptr<int[]> priority(new int[number_added_vars]);
    std::unique_ptr<int[]> priority_idx(new int[number_added_vars]);
    int num_priority_vars = 0;

    // Create new variables
    CHECK_STATUS(KN_add_vars(kc_, number_added_vars, NULL));
    for (int var_index = last_variable_index_; var_index < total_num_vars;
         ++var_index) {
      MPVariable* const var = solver_->variables_[var_index];
      DCHECK(!variable_is_extracted(var_index));
      set_variable_as_extracted(var_index, true);

      // Define the bounds and type of var
      idx_vars[var_index - last_variable_index_] = var_index;
      lb[var_index - last_variable_index_] = redefine_infinity_double(var->lb());
      ub[var_index - last_variable_index_] = redefine_infinity_double(var->ub());
      types[var_index - last_variable_index_] = ((mip_ && var->integer())
                                                  ? KN_VARTYPE_INTEGER
                                                  : KN_VARTYPE_CONTINUOUS);
      
      // Name the var
      CHECK_STATUS(KN_set_var_name(kc_, var_index, (char*)var->name().c_str()));

      // Branching priority 
      if (var->integer() && (var->branching_priority() != 0)) {
        priority_idx[num_priority_vars] = var_index;
        priority[num_priority_vars] = var->branching_priority();
        num_priority_vars++;
      }
    }

    CHECK_STATUS(KN_set_var_lobnds(kc_, number_added_vars, idx_vars.get(), lb.get()));
    CHECK_STATUS(KN_set_var_upbnds(kc_, number_added_vars, idx_vars.get(), ub.get()));
    CHECK_STATUS(KN_set_var_types(kc_, number_added_vars, idx_vars.get(), types.get()));
    CHECK_STATUS(KN_set_mip_branching_priorities(kc_, num_priority_vars, priority_idx.get(),
                                                 priority.get()));

    // Adds new variables to existing constraints.
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
    int const number_added_constraints = total_num_cons - last_constraint_index_;
    // Create new constraints
    CHECK_STATUS(KN_add_cons(kc_, number_added_constraints, NULL));
    // Counts the number of non zero linear term in case of update Knitro model
    int number_linear_terms = 0;

    // Add all constraints as a block
    std::unique_ptr<int[]> con_indexes(new int[total_num_vars * number_added_constraints]);
    std::unique_ptr<int[]> var_indexes(new int[total_num_vars * number_added_constraints]);
    std::unique_ptr<double[]> var_coefficients(new double[total_num_vars * number_added_constraints]);

    std::unique_ptr<int[]> idx_cons(new int[number_added_constraints]);
    std::unique_ptr<double[]> lb(new double[number_added_constraints]);
    std::unique_ptr<double[]> ub(new double[number_added_constraints]);

    for (int con_index = last_constraint_index_; con_index < total_num_cons; ++con_index) {
      MPConstraint* const ct = solver_->constraints_[con_index];
      DCHECK(!constraint_is_extracted(con_index));
      set_constraint_as_extracted(con_index, true);

      // Name the constraint
      CHECK_STATUS(KN_set_con_name(kc_, con_index, (char*)ct->name().c_str()));

      const auto& coeffs = ct->coefficients_;
      for (auto coeff : coeffs) {
        con_indexes[number_linear_terms] = con_index;
        var_indexes[number_linear_terms] = coeff.first->index();
        var_coefficients[number_linear_terms] = coeff.second;
        ++number_linear_terms;;
      }

      idx_cons[con_index - last_constraint_index_] = con_index;
      lb[con_index - last_constraint_index_] = redefine_infinity_double(ct->lb());
      ub[con_index - last_constraint_index_] = redefine_infinity_double(ct->ub());
    }

    CHECK_STATUS(KN_set_con_lobnds(kc_, number_added_constraints, idx_cons.get(), lb.get()));
    CHECK_STATUS(KN_set_con_upbnds(kc_, number_added_constraints, idx_cons.get(), ub.get()));

    if (number_linear_terms) {
      CHECK_STATUS(KN_add_con_linear_struct(kc_, number_linear_terms, 
                    con_indexes.get(), var_indexes.get(), var_coefficients.get()));
    }

    // if a new linear term is added, the Knitro model must be updated 
    if (number_linear_terms) CHECK_STATUS(KN_update(kc_));
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

// Save the existing locale, use the "C" locale to ensure that
// string -> double conversion is done ignoring the locale.
struct ScopedLocale {
  ScopedLocale() {
    oldLocale = std::setlocale(LC_NUMERIC, nullptr);
    auto newLocale = std::setlocale(LC_NUMERIC, "C");
    CHECK_EQ(std::string(newLocale), "C");
  }
  ~ScopedLocale() { std::setlocale(LC_NUMERIC, oldLocale); }

 private:
  const char* oldLocale;
};

bool KnitroInterface::SetSolverSpecificParametersAsString(
    const std::string& parameters) {
  if (parameters.empty()) {
    return true;
  }

  std::vector<std::pair<std::string, std::string> > paramAndValuePairList;

  std::stringstream ss(parameters);
  std::string paramName;
  while (std::getline(ss, paramName, ' ')) {
    std::string paramValue;
    if (std::getline(ss, paramValue, ' ')) {
      paramAndValuePairList.push_back(std::make_pair(paramName, paramValue));
    } else {
      LOG(ERROR) << "No value for parameter " << paramName << " : function "
                 << __FUNCTION__ << std::endl;
      return false;
    }
  }

  ScopedLocale locale;
  for (auto& paramAndValuePair : paramAndValuePairList) {
    auto matchingParamIter = param_map_.find(paramAndValuePair.first);
    if (matchingParamIter != param_map_.end()) {
      int param_id = param_map_[paramAndValuePair.first], param_type = 0;
      KN_get_param_type(kc_, param_id, &param_type);
      switch (param_type) {
        case KN_PARAMTYPE_INTEGER:
          KN_set_int_param(kc_, param_id, std::stoi(paramAndValuePair.second));
          break;
        case KN_PARAMTYPE_FLOAT:
          KN_set_double_param(kc_, param_id,
                              std::stod(paramAndValuePair.second));
          break;
        case KN_PARAMTYPE_STRING:
          KN_set_char_param(kc_, param_id, paramAndValuePair.second.c_str());
          break;
      }
      continue;
    }
    LOG(ERROR) << "Unknown parameter " << paramName << " : function "
               << __FUNCTION__ << std::endl;
    return false;
  }
  return true;
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
  int const length = 15;     // should contain the string termination character,
  char release[length + 1];  // checked but there is trouble if not allocated
                             // with an additional char

  CHECK_STATUS(KN_get_release(length, release));

  return absl::StrFormat("Knitro library version %s", release);
}

MPSolverInterface* BuildKnitroInterface(bool mip, MPSolver* const solver) {
  return new KnitroInterface(solver, mip);
}

}  // namespace operations_research