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

// Initial version of this code was provided by RTE

#include <algorithm>
#include <clocale>
#include <fstream>
#include <istream>
#include <limits>
#include <memory>
#include <mutex>
#include <string>
#include <numeric>

#include "absl/strings/str_format.h"
#include "ortools/base/logging.h"
#include "ortools/base/timer.h"
#include "ortools/linear_solver/linear_solver.h"
#include "ortools/xpress/environment.h"

#define XPRS_INTEGER 'I'
#define XPRS_CONTINUOUS 'C'

// The argument to this macro is the invocation of a XPRS function that
// returns a status. If the function returns non-zero the macro aborts
// the program with an appropriate error message.
#define CHECK_STATUS(s)    \
  do {                     \
    int const status_ = s; \
    CHECK_EQ(0, status_);  \
  } while (0)

namespace operations_research {
std::string getSolverVersion(XPRSprob const& prob) {
  // XPRS_VERSION gives the version number as MAJOR*100 + RELEASE.
  // It does not include the build number.
  int version;
  if (!prob || XPRSgetintcontrol(prob, XPRS_VERSION, &version))
    return "XPRESS library version unknown";

  int const major = version / 100;
  version -= major * 100;
  int const release = version;

  return absl::StrFormat("XPRESS library version %d.%02d", major, release);
}

// Apply the specified name=value setting to prob.
bool readParameter(XPRSprob const& prob, std::string const& name,
                   std::string const& value) {
  // We cannot set empty parameters.
  if (!value.size()) {
    LOG(DFATAL) << "Empty value for parameter '" << name << "' in "
                << getSolverVersion(prob);
    return false;
  }

  // Figure out the type of the control.
  int id, type;
  if (XPRSgetcontrolinfo(prob, name.c_str(), &id, &type) ||
      type == XPRS_TYPE_NOTDEFINED) {
    LOG(DFATAL) << "Unknown parameter '" << name << "' in "
                << getSolverVersion(prob);
    return false;
  }

  // Depending on the type, parse the text in value and apply it.
  std::stringstream v(value);
  v.imbue(std::locale("C"));
  switch (type) {
    case XPRS_TYPE_INT: {
      int i;
      v >> i;
      if (!v.eof()) {
        LOG(DFATAL) << "Failed to parse value '" << value
                    << "' for int parameter '" << name << "' in "
                    << getSolverVersion(prob);
        return false;
      }
      if (XPRSsetintcontrol(prob, id, i)) {
        LOG(DFATAL) << "Failed to set int parameter '" << name << "' to "
                    << value << " (" << i << ") in " << getSolverVersion(prob);
        return false;
      }
    } break;
    case XPRS_TYPE_INT64: {
      XPRSint64 i;
      v >> i;
      if (!v.eof()) {
        LOG(DFATAL) << "Failed to parse value '" << value
                    << "' for int64_t parameter '" << name << "' in "
                    << getSolverVersion(prob);
        return false;
      }
      if (XPRSsetintcontrol64(prob, id, i)) {
        LOG(DFATAL) << "Failed to set int64_t parameter '" << name << "' to "
                    << value << " (" << i << ") in " << getSolverVersion(prob);
        return false;
      }
    } break;
    case XPRS_TYPE_DOUBLE: {
      double d;
      v >> d;
      if (!v.eof()) {
        LOG(DFATAL) << "Failed to parse value '" << value
                    << "' for dbl parameter '" << name << "' in "
                    << getSolverVersion(prob);
        return false;
      }
      if (XPRSsetdblcontrol(prob, id, d)) {
        LOG(DFATAL) << "Failed to set double parameter '" << name << "' to "
                    << value << " (" << d << ") in " << getSolverVersion(prob);
        return false;
      }
    } break;
    default:
      // Note that string parameters are not supported at the moment since
      // we don't want to deal with potential encoding or escaping issues.
      LOG(DFATAL) << "Unsupported parameter type " << type << " for parameter '"
                  << name << "' in " << getSolverVersion(prob);
      return false;
  }

  return true;
}

void printError(const XPRSprob& mLp, int line) {
  char errmsg[512];
  XPRSgetlasterror(mLp, errmsg);
  VLOG(0) << absl::StrFormat("Function line %d did not execute correctly: %s\n",
                             line, errmsg);
  exit(0);
}

void XPRS_CC XpressIntSolCallbackImpl(XPRSprob cbprob, void* cbdata);

/**********************************************************************************\
* Name:         optimizermsg *
* Purpose:      Display Optimizer error messages and warnings. *
* Arguments:    const char *sMsg       Message string *
*               int nLen               Message length *
*               int nMsgLvl            Message type *
* Return Value: None *
\**********************************************************************************/
void XPRS_CC optimizermsg(XPRSprob prob, void* data, const char* sMsg, int nLen,
                          int nMsgLvl);

int getnumcols(const XPRSprob& mLp) {
  int nCols = 0;
  XPRSgetintattrib(mLp, XPRS_COLS, &nCols);
  return nCols;
}

int getnumrows(const XPRSprob& mLp) {
  int nRows = 0;
  XPRSgetintattrib(mLp, XPRS_ROWS, &nRows);
  return nRows;
}

int getitcnt(const XPRSprob& mLp) {
  int nIters = 0;
  XPRSgetintattrib(mLp, XPRS_SIMPLEXITER, &nIters);
  return nIters;
}

int getnodecnt(const XPRSprob& mLp) {
  int nNodes = 0;
  XPRSgetintattrib(mLp, XPRS_NODES, &nNodes);
  return nNodes;
}

int setobjoffset(const XPRSprob& mLp, double value) {
  // TODO detect xpress version
  static int indexes[1] = {-1};
  double values[1] = {-value};
  XPRSchgobj(mLp, 1, indexes, values);
  return 0;
}

void addhint(const XPRSprob& mLp, int length, const double solval[],
             const int colind[]) {
  // The OR-Tools API does not allow setting a name for the solution
  // passing NULL to XPRESS will have it generate a unique ID for the solution
  if (int status = XPRSaddmipsol(mLp, length, solval, colind, NULL)) {
    LOG(WARNING) << "Failed to set solution hint.";
  }
}

enum CUSTOM_INTERRUPT_REASON { CALLBACK_EXCEPTION = 0 };

void interruptXPRESS(XPRSprob& xprsProb, CUSTOM_INTERRUPT_REASON reason) {
  // Reason values below 1000 are reserved by XPRESS
  XPRSinterrupt(xprsProb, 1000 + reason);
}

// In case we need to return a double but don't have a value for that
// we just return a NaN.
#if !defined(XPRS_NAN)
#define XPRS_NAN std::numeric_limits<double>::quiet_NaN()
#endif

using std::unique_ptr;

class XpressMPCallbackContext : public MPCallbackContext {
  friend class XpressInterface;

 public:
  XpressMPCallbackContext(XPRSprob* xprsprob, MPCallbackEvent event,
                          int num_nodes)
      : xprsprob_(xprsprob),
        event_(event),
        num_nodes_(num_nodes),
        variable_values_(0) {};

  // Implementation of the interface.
  MPCallbackEvent Event() override { return event_; };
  bool CanQueryVariableValues() override;
  double VariableValue(const MPVariable* variable) override;
  void AddCut(const LinearRange& cutting_plane) override {
    LOG(WARNING) << "AddCut is not implemented yet in XPRESS interface";
  };
  void AddLazyConstraint(const LinearRange& lazy_constraint) override {
    LOG(WARNING)
        << "AddLazyConstraint is not implemented yet in XPRESS interface";
  };
  double SuggestSolution(
      const absl::flat_hash_map<const MPVariable*, double>& solution) override;
  int64_t NumExploredNodes() override { return num_nodes_; };

  // Call this method to update the internal state of the callback context
  // before passing it to MPCallback::RunCallback().
  // Returns true if the internal state has changed.
  bool UpdateFromXpressState(XPRSprob cbprob);

 private:
  XPRSprob* xprsprob_;
  MPCallbackEvent event_;
  std::vector<double>
      variable_values_;  // same order as MPVariable* elements in MPSolver
  int num_nodes_;
};

// Wraps the MPCallback in order to catch and store exceptions
class MPCallbackWrapper {
 public:
  explicit MPCallbackWrapper(MPCallback* callback) : callback_(callback) {};
  MPCallback* GetCallback() const { return callback_; }
  // Since our (C++) call-back functions are called from the XPRESS (C) code,
  // exceptions thrown in our call-back code are not caught by XPRESS.
  // We have to catch them, interrupt XPRESS, and log them after XPRESS is
  // effectively interrupted (ie after solve).
  void CatchException(XPRSprob cbprob) {
    exceptions_mutex_.lock();
    caught_exceptions_.push_back(std::current_exception());
    interruptXPRESS(cbprob, CALLBACK_EXCEPTION);
    exceptions_mutex_.unlock();
  }
  void LogCaughtExceptions() {
    exceptions_mutex_.lock();
    for (const std::exception_ptr& ex : caught_exceptions_) {
      try {
        std::rethrow_exception(ex);
      } catch (std::exception& ex) {
        // We don't want the interface to throw exceptions, plus it causes
        // SWIG issues in Java & Python. Instead, we'll only log them.
        // (The use cases where the user has to raise an exception inside their
        // call-back does not seem to be frequent, anyway.)
        LOG(ERROR) << "Caught exception during user-defined call-back: "
                   << ex.what();
      }
    }
    caught_exceptions_.clear();
    exceptions_mutex_.unlock();
  };

 private:
  MPCallback* callback_;
  std::vector<std::exception_ptr> caught_exceptions_;
  std::mutex exceptions_mutex_;
};

// For a model that is extracted to an instance of this class there is a
// 1:1 correspondence between MPVariable instances and XPRESS columns: the
// index of an extracted variable is the column index in the XPRESS model.
// Similar for instances of MPConstraint: the index of the constraint in
// the model is the row index in the XPRESS model.
class XpressInterface : public MPSolverInterface {
 public:
  // NOTE: 'mip' specifies the type of the problem (either continuous or
  //       mixed integer). This type is fixed for the lifetime of the
  //       instance. There are no dynamic changes to the model type.
  explicit XpressInterface(MPSolver* solver, bool mip);
  ~XpressInterface() override;

  // Sets the optimization direction (min/max).
  void SetOptimizationDirection(bool maximize) override;

  // ----- Solve -----
  // Solve the problem using the parameter values specified.
  MPSolver::ResultStatus Solve(MPSolverParameters const& param) override;

  // Writes the model.
  void Write(const std::string& filename) override;

  // ----- Model modifications and extraction -----
  // Resets extracted model
  void Reset() override;

  void SetVariableBounds(int var_index, double lb, double ub) override;
  void SetVariableInteger(int var_index, bool integer) override;
  void SetConstraintBounds(int row_index, double lb, double ub) override;

  void AddRowConstraint(MPConstraint* ct) override;
  void AddVariable(MPVariable* var) override;
  void SetCoefficient(MPConstraint* constraint, MPVariable const* variable,
                      double new_value, double old_value) override;

  // Clear a constraint from all its terms.
  void ClearConstraint(MPConstraint* constraint) override;
  // Change a coefficient in the linear objective
  void SetObjectiveCoefficient(MPVariable const* variable,
                               double coefficient) override;
  // Change the constant term in the linear objective.
  void SetObjectiveOffset(double value) override;
  // Clear the objective from all its terms.
  void ClearObjective() override;

  // ------ Query statistics on the solution and the solve ------
  // Number of simplex iterations
  virtual int64_t iterations() const;
  // Number of branch-and-bound nodes. Only available for discrete problems.
  virtual int64_t nodes() const;

  // Returns the basis status of a row.
  MPSolver::BasisStatus row_status(int constraint_index) const override;
  // Returns the basis status of a column.
  MPSolver::BasisStatus column_status(int variable_index) const override;

  // ----- Misc -----

  // Query problem type.
  // Remember that problem type is a static property that is set
  // in the constructor and never changed.
  bool IsContinuous() const override { return IsLP(); }
  bool IsLP() const override { return !mMip; }
  bool IsMIP() const override { return mMip; }

  void SetStartingLpBasis(
      const std::vector<MPSolver::BasisStatus>& variable_statuses,
      const std::vector<MPSolver::BasisStatus>& constraint_statuses) override;

  void ExtractNewVariables() override;
  void ExtractNewConstraints() override;
  void ExtractObjective() override;

  std::string SolverVersion() const override;

  void* underlying_solver() override { return reinterpret_cast<void*>(mLp); }

  double ComputeExactConditionNumber() const override {
    if (!IsContinuous()) {
      LOG(DFATAL) << "ComputeExactConditionNumber not implemented for"
                  << " XPRESS_MIXED_INTEGER_PROGRAMMING";
      return 0.0;
    }

    // TODO(user): Not yet working.
    LOG(DFATAL) << "ComputeExactConditionNumber not implemented for"
                << " XPRESS_LINEAR_PROGRAMMING";
    return 0.0;
  }

  void SetCallback(MPCallback* mp_callback) override;
  bool SupportsCallbacks() const override { return true; }

  bool InterruptSolve() override {
    if (mLp) XPRSinterrupt(mLp, XPRS_STOP_USER);
    return true;
  }

 protected:
  // Set all parameters in the underlying solver.
  void SetParameters(MPSolverParameters const& param) override;
  // Set each parameter in the underlying solver.
  void SetRelativeMipGap(double value) override;
  void SetPrimalTolerance(double value) override;
  void SetDualTolerance(double value) override;
  void SetPresolveMode(int value) override;
  void SetScalingMode(int value) override;
  void SetLpAlgorithm(int value) override;

  virtual bool ReadParameterFile(std::string const& filename);
  virtual std::string ValidFileExtensionForParameterFile() const;

 private:
  // Mark modeling object "out of sync". This implicitly invalidates
  // solution information as well. It is the counterpart of
  // MPSolverInterface::InvalidateSolutionSynchronization
  void InvalidateModelSynchronization() {
    mCstat.clear();
    mRstat.clear();
    sync_status_ = MUST_RELOAD;
  }
  // Adds a new feasible, infeasible or partial MIP solution for the problem to
  // the Optimizer. The hint is read in the MPSolver where the user set it using
  // SetHint()
  void AddSolutionHintToOptimizer();

  bool readParameters(std::istream& is, char sep);

 private:
  XPRSprob mLp;
  bool const mMip;
  // Incremental extraction.
  // Without incremental extraction we have to re-extract the model every
  // time we perform a solve. Due to the way the Reset() function is
  // implemented, this will lose MIP start or basis information from a
  // previous solve. On the other hand, if there is a significant changes
  // to the model then just re-extracting everything is usually faster than
  // keeping the low-level modeling object in sync with the high-level
  // variables/constraints.
  // Note that incremental extraction is particularly expensive in function
  // ExtractNewVariables() since there we must scan _all_ old constraints
  // and update them with respect to the new variables.
  bool const supportIncrementalExtraction;

  // Use slow and immediate updates or try to do bulk updates.
  // For many updates to the model we have the option to either perform
  // the update immediately with a potentially slow operation or to
  // just mark the low-level modeling object out of sync and re-extract
  // the model later.
  enum SlowUpdates {
    SlowSetCoefficient = 0x0001,
    SlowClearConstraint = 0x0002,
    SlowSetObjectiveCoefficient = 0x0004,
    SlowClearObjective = 0x0008,
    SlowSetConstraintBounds = 0x0010,
    SlowSetVariableInteger = 0x0020,
    SlowSetVariableBounds = 0x0040,
    SlowUpdatesAll = 0xffff
  } const slowUpdates;
  // XPRESS has no method to query the basis status of a single variable.
  // Hence, we query the status only once and cache the array. This is
  // much faster in case the basis status of more than one row/column
  // is required.

  // TODO
  std::vector<int> mutable mCstat;
  std::vector<int> mutable mRstat;

  std::vector<int> mutable initial_variables_basis_status_;
  std::vector<int> mutable initial_constraint_basis_status_;

  // Setup the right-hand side of a constraint from its lower and upper bound.
  static void MakeRhs(double lb, double ub, double& rhs, char& sense,
                      double& range);

  std::map<std::string, int>& mapStringControls_;
  std::map<std::string, int>& mapDoubleControls_;
  std::map<std::string, int>& mapIntegerControls_;
  std::map<std::string, int>& mapInteger64Controls_;

  bool SetSolverSpecificParametersAsString(
      const std::string& parameters) override;
  MPCallback* callback_ = nullptr;
};

// Transform MPSolver basis status to XPRESS status
static int MPSolverToXpressBasisStatus(
    MPSolver::BasisStatus mpsolver_basis_status);
// Transform XPRESS basis status to MPSolver basis status.
static MPSolver::BasisStatus XpressToMPSolverBasisStatus(
    int xpress_basis_status);

static std::map<std::string, int>& getMapStringControls() {
  static std::map<std::string, int> mapControls = {
      {"MPSRHSNAME", XPRS_MPSRHSNAME},
      {"MPSOBJNAME", XPRS_MPSOBJNAME},
      {"MPSRANGENAME", XPRS_MPSRANGENAME},
      {"MPSBOUNDNAME", XPRS_MPSBOUNDNAME},
      {"OUTPUTMASK", XPRS_OUTPUTMASK},
      {"TUNERMETHODFILE", XPRS_TUNERMETHODFILE},
      {"TUNEROUTPUTPATH", XPRS_TUNEROUTPUTPATH},
      {"TUNERSESSIONNAME", XPRS_TUNERSESSIONNAME},
      {"COMPUTEEXECSERVICE", XPRS_COMPUTEEXECSERVICE},
  };
  return mapControls;
}

static std::map<std::string, int>& getMapDoubleControls() {
  static std::map<std::string, int> mapControls = {
      {"MAXCUTTIME", XPRS_MAXCUTTIME},
      {"MAXSTALLTIME", XPRS_MAXSTALLTIME},
      {"TUNERMAXTIME", XPRS_TUNERMAXTIME},
      {"MATRIXTOL", XPRS_MATRIXTOL},
      {"PIVOTTOL", XPRS_PIVOTTOL},
      {"FEASTOL", XPRS_FEASTOL},
      {"OUTPUTTOL", XPRS_OUTPUTTOL},
      {"SOSREFTOL", XPRS_SOSREFTOL},
      {"OPTIMALITYTOL", XPRS_OPTIMALITYTOL},
      {"ETATOL", XPRS_ETATOL},
      {"RELPIVOTTOL", XPRS_RELPIVOTTOL},
      {"MIPTOL", XPRS_MIPTOL},
      {"MIPTOLTARGET", XPRS_MIPTOLTARGET},
      {"BARPERTURB", XPRS_BARPERTURB},
      {"MIPADDCUTOFF", XPRS_MIPADDCUTOFF},
      {"MIPABSCUTOFF", XPRS_MIPABSCUTOFF},
      {"MIPRELCUTOFF", XPRS_MIPRELCUTOFF},
      {"PSEUDOCOST", XPRS_PSEUDOCOST},
      {"PENALTY", XPRS_PENALTY},
      {"BIGM", XPRS_BIGM},
      {"MIPABSSTOP", XPRS_MIPABSSTOP},
      {"MIPRELSTOP", XPRS_MIPRELSTOP},
      {"CROSSOVERACCURACYTOL", XPRS_CROSSOVERACCURACYTOL},
      {"PRIMALPERTURB", XPRS_PRIMALPERTURB},
      {"DUALPERTURB", XPRS_DUALPERTURB},
      {"BAROBJSCALE", XPRS_BAROBJSCALE},
      {"BARRHSSCALE", XPRS_BARRHSSCALE},
      {"CHOLESKYTOL", XPRS_CHOLESKYTOL},
      {"BARGAPSTOP", XPRS_BARGAPSTOP},
      {"BARDUALSTOP", XPRS_BARDUALSTOP},
      {"BARPRIMALSTOP", XPRS_BARPRIMALSTOP},
      {"BARSTEPSTOP", XPRS_BARSTEPSTOP},
      {"ELIMTOL", XPRS_ELIMTOL},
      {"MARKOWITZTOL", XPRS_MARKOWITZTOL},
      {"MIPABSGAPNOTIFY", XPRS_MIPABSGAPNOTIFY},
      {"MIPRELGAPNOTIFY", XPRS_MIPRELGAPNOTIFY},
      {"BARLARGEBOUND", XPRS_BARLARGEBOUND},
      {"PPFACTOR", XPRS_PPFACTOR},
      {"REPAIRINDEFINITEQMAX", XPRS_REPAIRINDEFINITEQMAX},
      {"BARGAPTARGET", XPRS_BARGAPTARGET},
      {"DUMMYCONTROL", XPRS_DUMMYCONTROL},
      {"BARSTARTWEIGHT", XPRS_BARSTARTWEIGHT},
      {"BARFREESCALE", XPRS_BARFREESCALE},
      {"SBEFFORT", XPRS_SBEFFORT},
      {"HEURDIVERANDOMIZE", XPRS_HEURDIVERANDOMIZE},
      {"HEURSEARCHEFFORT", XPRS_HEURSEARCHEFFORT},
      {"CUTFACTOR", XPRS_CUTFACTOR},
      {"EIGENVALUETOL", XPRS_EIGENVALUETOL},
      {"INDLINBIGM", XPRS_INDLINBIGM},
      {"TREEMEMORYSAVINGTARGET", XPRS_TREEMEMORYSAVINGTARGET},
      {"INDPRELINBIGM", XPRS_INDPRELINBIGM},
      {"RELAXTREEMEMORYLIMIT", XPRS_RELAXTREEMEMORYLIMIT},
      {"MIPABSGAPNOTIFYOBJ", XPRS_MIPABSGAPNOTIFYOBJ},
      {"MIPABSGAPNOTIFYBOUND", XPRS_MIPABSGAPNOTIFYBOUND},
      {"PRESOLVEMAXGROW", XPRS_PRESOLVEMAXGROW},
      {"HEURSEARCHTARGETSIZE", XPRS_HEURSEARCHTARGETSIZE},
      {"CROSSOVERRELPIVOTTOL", XPRS_CROSSOVERRELPIVOTTOL},
      {"CROSSOVERRELPIVOTTOLSAFE", XPRS_CROSSOVERRELPIVOTTOLSAFE},
      {"DETLOGFREQ", XPRS_DETLOGFREQ},
      {"MAXIMPLIEDBOUND", XPRS_MAXIMPLIEDBOUND},
      {"FEASTOLTARGET", XPRS_FEASTOLTARGET},
      {"OPTIMALITYTOLTARGET", XPRS_OPTIMALITYTOLTARGET},
      {"PRECOMPONENTSEFFORT", XPRS_PRECOMPONENTSEFFORT},
      {"LPLOGDELAY", XPRS_LPLOGDELAY},
      {"HEURDIVEITERLIMIT", XPRS_HEURDIVEITERLIMIT},
      {"BARKERNEL", XPRS_BARKERNEL},
      {"FEASTOLPERTURB", XPRS_FEASTOLPERTURB},
      {"CROSSOVERFEASWEIGHT", XPRS_CROSSOVERFEASWEIGHT},
      {"LUPIVOTTOL", XPRS_LUPIVOTTOL},
      {"MIPRESTARTGAPTHRESHOLD", XPRS_MIPRESTARTGAPTHRESHOLD},
      {"NODEPROBINGEFFORT", XPRS_NODEPROBINGEFFORT},
      {"INPUTTOL", XPRS_INPUTTOL},
      {"MIPRESTARTFACTOR", XPRS_MIPRESTARTFACTOR},
      {"BAROBJPERTURB", XPRS_BAROBJPERTURB},
      {"CPIALPHA", XPRS_CPIALPHA},
      {"GLOBALBOUNDINGBOX", XPRS_GLOBALBOUNDINGBOX},
      {"TIMELIMIT", XPRS_TIMELIMIT},
      {"SOLTIMELIMIT", XPRS_SOLTIMELIMIT},
      {"REPAIRINFEASTIMELIMIT", XPRS_REPAIRINFEASTIMELIMIT},
  };
  return mapControls;
}

static std::map<std::string, int>& getMapIntControls() {
  static std::map<std::string, int> mapControls = {
      {"EXTRAROWS", XPRS_EXTRAROWS},
      {"EXTRACOLS", XPRS_EXTRACOLS},
      {"LPITERLIMIT", XPRS_LPITERLIMIT},
      {"LPLOG", XPRS_LPLOG},
      {"SCALING", XPRS_SCALING},
      {"PRESOLVE", XPRS_PRESOLVE},
      {"CRASH", XPRS_CRASH},
      {"PRICINGALG", XPRS_PRICINGALG},
      {"INVERTFREQ", XPRS_INVERTFREQ},
      {"INVERTMIN", XPRS_INVERTMIN},
      {"MAXNODE", XPRS_MAXNODE},
      {"MAXTIME", XPRS_MAXTIME},
      {"MAXMIPSOL", XPRS_MAXMIPSOL},
      {"SIFTPASSES", XPRS_SIFTPASSES},
      {"DEFAULTALG", XPRS_DEFAULTALG},
      {"VARSELECTION", XPRS_VARSELECTION},
      {"NODESELECTION", XPRS_NODESELECTION},
      {"BACKTRACK", XPRS_BACKTRACK},
      {"MIPLOG", XPRS_MIPLOG},
      {"KEEPNROWS", XPRS_KEEPNROWS},
      {"MPSECHO", XPRS_MPSECHO},
      {"MAXPAGELINES", XPRS_MAXPAGELINES},
      {"OUTPUTLOG", XPRS_OUTPUTLOG},
      {"BARSOLUTION", XPRS_BARSOLUTION},
      {"CACHESIZE", XPRS_CACHESIZE},
      {"CROSSOVER", XPRS_CROSSOVER},
      {"BARITERLIMIT", XPRS_BARITERLIMIT},
      {"CHOLESKYALG", XPRS_CHOLESKYALG},
      {"BAROUTPUT", XPRS_BAROUTPUT},
      {"EXTRAMIPENTS", XPRS_EXTRAMIPENTS},
      {"REFACTOR", XPRS_REFACTOR},
      {"BARTHREADS", XPRS_BARTHREADS},
      {"KEEPBASIS", XPRS_KEEPBASIS},
      {"CROSSOVEROPS", XPRS_CROSSOVEROPS},
      {"VERSION", XPRS_VERSION},
      {"CROSSOVERTHREADS", XPRS_CROSSOVERTHREADS},
      {"BIGMMETHOD", XPRS_BIGMMETHOD},
      {"MPSNAMELENGTH", XPRS_MPSNAMELENGTH},
      {"ELIMFILLIN", XPRS_ELIMFILLIN},
      {"PRESOLVEOPS", XPRS_PRESOLVEOPS},
      {"MIPPRESOLVE", XPRS_MIPPRESOLVE},
      {"MIPTHREADS", XPRS_MIPTHREADS},
      {"BARORDER", XPRS_BARORDER},
      {"BREADTHFIRST", XPRS_BREADTHFIRST},
      {"AUTOPERTURB", XPRS_AUTOPERTURB},
      {"DENSECOLLIMIT", XPRS_DENSECOLLIMIT},
      {"CALLBACKFROMMASTERTHREAD", XPRS_CALLBACKFROMMASTERTHREAD},
      {"MAXMCOEFFBUFFERELEMS", XPRS_MAXMCOEFFBUFFERELEMS},
      {"REFINEOPS", XPRS_REFINEOPS},
      {"LPREFINEITERLIMIT", XPRS_LPREFINEITERLIMIT},
      {"MIPREFINEITERLIMIT", XPRS_MIPREFINEITERLIMIT},
      {"DUALIZEOPS", XPRS_DUALIZEOPS},
      {"CROSSOVERITERLIMIT", XPRS_CROSSOVERITERLIMIT},
      {"PREBASISRED", XPRS_PREBASISRED},
      {"PRESORT", XPRS_PRESORT},
      {"PREPERMUTE", XPRS_PREPERMUTE},
      {"PREPERMUTESEED", XPRS_PREPERMUTESEED},
      {"MAXMEMORYSOFT", XPRS_MAXMEMORYSOFT},
      {"CUTFREQ", XPRS_CUTFREQ},
      {"SYMSELECT", XPRS_SYMSELECT},
      {"SYMMETRY", XPRS_SYMMETRY},
      {"MAXMEMORYHARD", XPRS_MAXMEMORYHARD},
      {"MIQCPALG", XPRS_MIQCPALG},
      {"QCCUTS", XPRS_QCCUTS},
      {"QCROOTALG", XPRS_QCROOTALG},
      {"PRECONVERTSEPARABLE", XPRS_PRECONVERTSEPARABLE},
      {"ALGAFTERNETWORK", XPRS_ALGAFTERNETWORK},
      {"TRACE", XPRS_TRACE},
      {"MAXIIS", XPRS_MAXIIS},
      {"CPUTIME", XPRS_CPUTIME},
      {"COVERCUTS", XPRS_COVERCUTS},
      {"GOMCUTS", XPRS_GOMCUTS},
      {"LPFOLDING", XPRS_LPFOLDING},
      {"MPSFORMAT", XPRS_MPSFORMAT},
      {"CUTSTRATEGY", XPRS_CUTSTRATEGY},
      {"CUTDEPTH", XPRS_CUTDEPTH},
      {"TREECOVERCUTS", XPRS_TREECOVERCUTS},
      {"TREEGOMCUTS", XPRS_TREEGOMCUTS},
      {"CUTSELECT", XPRS_CUTSELECT},
      {"TREECUTSELECT", XPRS_TREECUTSELECT},
      {"DUALIZE", XPRS_DUALIZE},
      {"DUALGRADIENT", XPRS_DUALGRADIENT},
      {"SBITERLIMIT", XPRS_SBITERLIMIT},
      {"SBBEST", XPRS_SBBEST},
      {"BARINDEFLIMIT", XPRS_BARINDEFLIMIT},
      {"HEURFREQ", XPRS_HEURFREQ},
      {"HEURDEPTH", XPRS_HEURDEPTH},
      {"HEURMAXSOL", XPRS_HEURMAXSOL},
      {"HEURNODES", XPRS_HEURNODES},
      {"LNPBEST", XPRS_LNPBEST},
      {"LNPITERLIMIT", XPRS_LNPITERLIMIT},
      {"BRANCHCHOICE", XPRS_BRANCHCHOICE},
      {"BARREGULARIZE", XPRS_BARREGULARIZE},
      {"SBSELECT", XPRS_SBSELECT},
      {"LOCALCHOICE", XPRS_LOCALCHOICE},
      {"LOCALBACKTRACK", XPRS_LOCALBACKTRACK},
      {"DUALSTRATEGY", XPRS_DUALSTRATEGY},
      {"L1CACHE", XPRS_L1CACHE},
      {"HEURDIVESTRATEGY", XPRS_HEURDIVESTRATEGY},
      {"HEURSELECT", XPRS_HEURSELECT},
      {"BARSTART", XPRS_BARSTART},
      {"PRESOLVEPASSES", XPRS_PRESOLVEPASSES},
      {"BARNUMSTABILITY", XPRS_BARNUMSTABILITY},
      {"BARORDERTHREADS", XPRS_BARORDERTHREADS},
      {"EXTRASETS", XPRS_EXTRASETS},
      {"FEASIBILITYPUMP", XPRS_FEASIBILITYPUMP},
      {"PRECOEFELIM", XPRS_PRECOEFELIM},
      {"PREDOMCOL", XPRS_PREDOMCOL},
      {"HEURSEARCHFREQ", XPRS_HEURSEARCHFREQ},
      {"HEURDIVESPEEDUP", XPRS_HEURDIVESPEEDUP},
      {"SBESTIMATE", XPRS_SBESTIMATE},
      {"BARCORES", XPRS_BARCORES},
      {"MAXCHECKSONMAXTIME", XPRS_MAXCHECKSONMAXTIME},
      {"MAXCHECKSONMAXCUTTIME", XPRS_MAXCHECKSONMAXCUTTIME},
      {"HISTORYCOSTS", XPRS_HISTORYCOSTS},
      {"ALGAFTERCROSSOVER", XPRS_ALGAFTERCROSSOVER},
      {"MUTEXCALLBACKS", XPRS_MUTEXCALLBACKS},
      {"BARCRASH", XPRS_BARCRASH},
      {"HEURDIVESOFTROUNDING", XPRS_HEURDIVESOFTROUNDING},
      {"HEURSEARCHROOTSELECT", XPRS_HEURSEARCHROOTSELECT},
      {"HEURSEARCHTREESELECT", XPRS_HEURSEARCHTREESELECT},
      {"MPS18COMPATIBLE", XPRS_MPS18COMPATIBLE},
      {"ROOTPRESOLVE", XPRS_ROOTPRESOLVE},
      {"CROSSOVERDRP", XPRS_CROSSOVERDRP},
      {"FORCEOUTPUT", XPRS_FORCEOUTPUT},
      {"PRIMALOPS", XPRS_PRIMALOPS},
      {"DETERMINISTIC", XPRS_DETERMINISTIC},
      {"PREPROBING", XPRS_PREPROBING},
      {"TREEMEMORYLIMIT", XPRS_TREEMEMORYLIMIT},
      {"TREECOMPRESSION", XPRS_TREECOMPRESSION},
      {"TREEDIAGNOSTICS", XPRS_TREEDIAGNOSTICS},
      {"MAXTREEFILESIZE", XPRS_MAXTREEFILESIZE},
      {"PRECLIQUESTRATEGY", XPRS_PRECLIQUESTRATEGY},
      {"REPAIRINFEASMAXTIME", XPRS_REPAIRINFEASMAXTIME},
      {"IFCHECKCONVEXITY", XPRS_IFCHECKCONVEXITY},
      {"PRIMALUNSHIFT", XPRS_PRIMALUNSHIFT},
      {"REPAIRINDEFINITEQ", XPRS_REPAIRINDEFINITEQ},
      {"MIPRAMPUP", XPRS_MIPRAMPUP},
      {"MAXLOCALBACKTRACK", XPRS_MAXLOCALBACKTRACK},
      {"USERSOLHEURISTIC", XPRS_USERSOLHEURISTIC},
      {"FORCEPARALLELDUAL", XPRS_FORCEPARALLELDUAL},
      {"BACKTRACKTIE", XPRS_BACKTRACKTIE},
      {"BRANCHDISJ", XPRS_BRANCHDISJ},
      {"MIPFRACREDUCE", XPRS_MIPFRACREDUCE},
      {"CONCURRENTTHREADS", XPRS_CONCURRENTTHREADS},
      {"MAXSCALEFACTOR", XPRS_MAXSCALEFACTOR},
      {"HEURTHREADS", XPRS_HEURTHREADS},
      {"THREADS", XPRS_THREADS},
      {"HEURBEFORELP", XPRS_HEURBEFORELP},
      {"PREDOMROW", XPRS_PREDOMROW},
      {"BRANCHSTRUCTURAL", XPRS_BRANCHSTRUCTURAL},
      {"QUADRATICUNSHIFT", XPRS_QUADRATICUNSHIFT},
      {"BARPRESOLVEOPS", XPRS_BARPRESOLVEOPS},
      {"QSIMPLEXOPS", XPRS_QSIMPLEXOPS},
      {"MIPRESTART", XPRS_MIPRESTART},
      {"CONFLICTCUTS", XPRS_CONFLICTCUTS},
      {"PREPROTECTDUAL", XPRS_PREPROTECTDUAL},
      {"CORESPERCPU", XPRS_CORESPERCPU},
      {"RESOURCESTRATEGY", XPRS_RESOURCESTRATEGY},
      {"CLAMPING", XPRS_CLAMPING},
      {"SLEEPONTHREADWAIT", XPRS_SLEEPONTHREADWAIT},
      {"PREDUPROW", XPRS_PREDUPROW},
      {"CPUPLATFORM", XPRS_CPUPLATFORM},
      {"BARALG", XPRS_BARALG},
      {"SIFTING", XPRS_SIFTING},
      {"LPLOGSTYLE", XPRS_LPLOGSTYLE},
      {"RANDOMSEED", XPRS_RANDOMSEED},
      {"TREEQCCUTS", XPRS_TREEQCCUTS},
      {"PRELINDEP", XPRS_PRELINDEP},
      {"DUALTHREADS", XPRS_DUALTHREADS},
      {"PREOBJCUTDETECT", XPRS_PREOBJCUTDETECT},
      {"PREBNDREDQUAD", XPRS_PREBNDREDQUAD},
      {"PREBNDREDCONE", XPRS_PREBNDREDCONE},
      {"PRECOMPONENTS", XPRS_PRECOMPONENTS},
      {"MAXMIPTASKS", XPRS_MAXMIPTASKS},
      {"MIPTERMINATIONMETHOD", XPRS_MIPTERMINATIONMETHOD},
      {"PRECONEDECOMP", XPRS_PRECONEDECOMP},
      {"HEURFORCESPECIALOBJ", XPRS_HEURFORCESPECIALOBJ},
      {"HEURSEARCHROOTCUTFREQ", XPRS_HEURSEARCHROOTCUTFREQ},
      {"PREELIMQUAD", XPRS_PREELIMQUAD},
      {"PREIMPLICATIONS", XPRS_PREIMPLICATIONS},
      {"TUNERMODE", XPRS_TUNERMODE},
      {"TUNERMETHOD", XPRS_TUNERMETHOD},
      {"TUNERTARGET", XPRS_TUNERTARGET},
      {"TUNERTHREADS", XPRS_TUNERTHREADS},
      {"TUNERHISTORY", XPRS_TUNERHISTORY},
      {"TUNERPERMUTE", XPRS_TUNERPERMUTE},
      {"TUNERVERBOSE", XPRS_TUNERVERBOSE},
      {"TUNEROUTPUT", XPRS_TUNEROUTPUT},
      {"PREANALYTICCENTER", XPRS_PREANALYTICCENTER},
      {"NETCUTS", XPRS_NETCUTS},
      {"LPFLAGS", XPRS_LPFLAGS},
      {"MIPKAPPAFREQ", XPRS_MIPKAPPAFREQ},
      {"OBJSCALEFACTOR", XPRS_OBJSCALEFACTOR},
      {"TREEFILELOGINTERVAL", XPRS_TREEFILELOGINTERVAL},
      {"IGNORECONTAINERCPULIMIT", XPRS_IGNORECONTAINERCPULIMIT},
      {"IGNORECONTAINERMEMORYLIMIT", XPRS_IGNORECONTAINERMEMORYLIMIT},
      {"MIPDUALREDUCTIONS", XPRS_MIPDUALREDUCTIONS},
      {"GENCONSDUALREDUCTIONS", XPRS_GENCONSDUALREDUCTIONS},
      {"PWLDUALREDUCTIONS", XPRS_PWLDUALREDUCTIONS},
      {"BARFAILITERLIMIT", XPRS_BARFAILITERLIMIT},
      {"AUTOSCALING", XPRS_AUTOSCALING},
      {"GENCONSABSTRANSFORMATION", XPRS_GENCONSABSTRANSFORMATION},
      {"COMPUTEJOBPRIORITY", XPRS_COMPUTEJOBPRIORITY},
      {"PREFOLDING", XPRS_PREFOLDING},
      {"NETSTALLLIMIT", XPRS_NETSTALLLIMIT},
      {"SERIALIZEPREINTSOL", XPRS_SERIALIZEPREINTSOL},
      {"NUMERICALEMPHASIS", XPRS_NUMERICALEMPHASIS},
      {"PWLNONCONVEXTRANSFORMATION", XPRS_PWLNONCONVEXTRANSFORMATION},
      {"MIPCOMPONENTS", XPRS_MIPCOMPONENTS},
      {"MIPCONCURRENTNODES", XPRS_MIPCONCURRENTNODES},
      {"MIPCONCURRENTSOLVES", XPRS_MIPCONCURRENTSOLVES},
      {"OUTPUTCONTROLS", XPRS_OUTPUTCONTROLS},
      {"SIFTSWITCH", XPRS_SIFTSWITCH},
      {"HEUREMPHASIS", XPRS_HEUREMPHASIS},
      {"COMPUTEMATX", XPRS_COMPUTEMATX},
      {"COMPUTEMATX_IIS", XPRS_COMPUTEMATX_IIS},
      {"COMPUTEMATX_IISMAXTIME", XPRS_COMPUTEMATX_IISMAXTIME},
      {"BARREFITER", XPRS_BARREFITER},
      {"COMPUTELOG", XPRS_COMPUTELOG},
      {"SIFTPRESOLVEOPS", XPRS_SIFTPRESOLVEOPS},
      {"CHECKINPUTDATA", XPRS_CHECKINPUTDATA},
      {"ESCAPENAMES", XPRS_ESCAPENAMES},
      {"IOTIMEOUT", XPRS_IOTIMEOUT},
      {"AUTOCUTTING", XPRS_AUTOCUTTING},
      {"CALLBACKCHECKTIMEDELAY", XPRS_CALLBACKCHECKTIMEDELAY},
      {"MULTIOBJOPS", XPRS_MULTIOBJOPS},
      {"MULTIOBJLOG", XPRS_MULTIOBJLOG},
      {"GLOBALSPATIALBRANCHIFPREFERORIG", XPRS_GLOBALSPATIALBRANCHIFPREFERORIG},
      {"PRECONFIGURATION", XPRS_PRECONFIGURATION},
      {"FEASIBILITYJUMP", XPRS_FEASIBILITYJUMP},
  };
  return mapControls;
}

static std::map<std::string, int>& getMapInt64Controls() {
  static std::map<std::string, int> mapControls = {
      {"EXTRAELEMS", XPRS_EXTRAELEMS},
      {"EXTRASETELEMS", XPRS_EXTRASETELEMS},
  };
  return mapControls;
}

// Creates an LP/MIP instance.
XpressInterface::XpressInterface(MPSolver* const solver, bool mip)
    : MPSolverInterface(solver),
      mLp(nullptr),
      mMip(mip),
      supportIncrementalExtraction(false),
      slowUpdates(SlowClearObjective),
      mapStringControls_(getMapStringControls()),
      mapDoubleControls_(getMapDoubleControls()),
      mapIntegerControls_(getMapIntControls()),
      mapInteger64Controls_(getMapInt64Controls()) {
  bool correctlyLoaded = initXpressEnv();
  CHECK(correctlyLoaded);
  int status = XPRScreateprob(&mLp);
  CHECK_STATUS(status);
  DCHECK(mLp != nullptr);  // should not be NULL if status=0
  int nReturn = XPRSaddcbmessage(mLp, optimizermsg, (void*)this, 0);
  CHECK_STATUS(
      XPRSchgobjsense(mLp, maximize_ ? XPRS_OBJ_MAXIMIZE : XPRS_OBJ_MINIMIZE));
}

XpressInterface::~XpressInterface() {
  CHECK_STATUS(XPRSdestroyprob(mLp));
  CHECK_STATUS(XPRSfree());
}

std::string XpressInterface::SolverVersion() const {
  // We prefer XPRSversionnumber() over XPRSversion() since the
  // former will never pose any encoding issues.
  int version = 0;
  CHECK_STATUS(XPRSgetintcontrol(mLp, XPRS_VERSION, &version));

  int const major = version / 1000000;
  version -= major * 1000000;
  int const release = version / 10000;
  version -= release * 10000;
  int const mod = version / 100;
  version -= mod * 100;
  int const fix = version;

  return absl::StrFormat("XPRESS library version %d.%02d.%02d.%02d", major,
                         release, mod, fix);
}

// ------ Model modifications and extraction -----

void XpressInterface::Reset() {
  int nRows = getnumrows(mLp);
  std::vector<int> rows(nRows);
  std::iota(rows.begin(), rows.end(), 0);
  int nCols = getnumcols(mLp);
  std::vector<int> cols(nCols);
  std::iota(cols.begin(), cols.end(), 0);
  XPRSdelrows(mLp, nRows, rows.data());
  XPRSdelcols(mLp, nCols, cols.data());
  XPRSdelobj(mLp, 0);
  ResetExtractionInformation();
  mCstat.clear();
  mRstat.clear();
}

void XpressInterface::SetOptimizationDirection(bool maximize) {
  InvalidateSolutionSynchronization();
  XPRSchgobjsense(mLp, maximize ? XPRS_OBJ_MAXIMIZE : XPRS_OBJ_MINIMIZE);
}

void XpressInterface::SetVariableBounds(int var_index, double lb, double ub) {
  InvalidateSolutionSynchronization();

  // Changing the bounds of a variable is fast. However, doing this for
  // many variables may still be slow. So we don't perform the update by
  // default. However, if we support incremental extraction
  // (supportIncrementalExtraction is true) then we MUST perform the
  // update here, or we will lose it.

  if (!supportIncrementalExtraction && !(slowUpdates & SlowSetVariableBounds)) {
    InvalidateModelSynchronization();
  } else {
    if (variable_is_extracted(var_index)) {
      // Variable has already been extracted, so we must modify the
      // modeling object.
      DCHECK_LT(var_index, last_variable_index_);
      char const lu[2] = {'L', 'U'};
      double const bd[2] = {lb, ub};
      int const idx[2] = {var_index, var_index};
      CHECK_STATUS(XPRSchgbounds(mLp, 2, idx, lu, bd));
    } else {
      // Variable is not yet extracted. It is sufficient to just mark
      // the modeling object "out of sync"
      InvalidateModelSynchronization();
    }
  }
}

// Modifies integrality of an extracted variable.
void XpressInterface::SetVariableInteger(int var_index, bool integer) {
  InvalidateSolutionSynchronization();

  // NOTE: The type of the model (continuous or mixed integer) is
  //       defined once and for all in the constructor. There are no
  //       dynamic changes to the model type.

  // Changing the type of a variable should be fast. Still, doing all
  // updates in one big chunk right before solve() is usually faster.
  // However, if we support incremental extraction
  // (supportIncrementalExtraction is true) then we MUST change the
  // type of extracted variables here.

  if (!supportIncrementalExtraction &&
      !(slowUpdates & SlowSetVariableInteger)) {
    InvalidateModelSynchronization();
  } else {
    if (mMip) {
      if (variable_is_extracted(var_index)) {
        // Variable is extracted. Change the type immediately.
        // TODO: Should we check the current type and don't do anything
        //       in case the type does not change?
        DCHECK_LE(var_index, getnumcols(mLp));
        char const type = integer ? XPRS_INTEGER : XPRS_CONTINUOUS;
        CHECK_STATUS(XPRSchgcoltype(mLp, 1, &var_index, &type));
      } else {
        InvalidateModelSynchronization();
      }
    } else {
      LOG(DFATAL)
          << "Attempt to change variable to integer in non-MIP problem!";
    }
  }
}

// Setup the right-hand side of a constraint.
void XpressInterface::MakeRhs(double lb, double ub, double& rhs, char& sense,
                              double& range) {
  if (lb == ub) {
    // Both bounds are equal -> this is an equality constraint
    rhs = lb;
    range = 0.0;
    sense = 'E';
  } else if (lb > XPRS_MINUSINFINITY && ub < XPRS_PLUSINFINITY) {
    // Both bounds are finite -> this is a ranged constraint
    // The value of a ranged constraint is allowed to be in
    //   [ rhs-rngval, rhs ]
    // Xpress does not support contradictory bounds. Instead the sign on
    // rndval is always ignored.
    if (lb > ub) {
      // TODO check if this is ok for the user
      LOG(DFATAL) << "XPRESS does not support contradictory bounds on range "
                     "constraints! ["
                  << lb << ", " << ub << "] will be converted to " << ub << ", "
                  << (ub - std::abs(ub - lb)) << "]";
    }
    rhs = ub;
    range = std::abs(ub - lb);  // This happens implicitly by XPRSaddrows()
    sense = 'R';
  } else if (ub < XPRS_PLUSINFINITY || (std::abs(ub) == XPRS_PLUSINFINITY &&
                                        std::abs(lb) > XPRS_PLUSINFINITY)) {
    // Finite upper, infinite lower bound -> this is a <= constraint
    rhs = ub;
    range = 0.0;
    sense = 'L';
  } else if (lb > XPRS_MINUSINFINITY || (std::abs(lb) == XPRS_PLUSINFINITY &&
                                         std::abs(ub) > XPRS_PLUSINFINITY)) {
    // Finite lower, infinite upper bound -> this is a >= constraint
    rhs = lb;
    range = 0.0;
    sense = 'G';
  } else {
    // Lower and upper bound are both infinite.
    // This is used for example in .mps files to specify alternate
    // objective functions.
    // A free row is denoted by sense 'N' and we can specify arbitrary
    // right-hand sides since they are ignored anyway. We just pick the
    // bound with smaller absolute value.
    DCHECK_GE(std::abs(lb), XPRS_PLUSINFINITY);
    DCHECK_GE(std::abs(ub), XPRS_PLUSINFINITY);
    if (std::abs(lb) < std::abs(ub))
      rhs = lb;
    else
      rhs = ub;
    range = 0.0;
    sense = 'N';
  }
}

void XpressInterface::SetConstraintBounds(int index, double lb, double ub) {
  InvalidateSolutionSynchronization();

  // Changing rhs, sense, or range of a constraint is not too slow.
  // Still, doing all the updates in one large operation is faster.
  // Note however that if we do not want to re-extract the full model
  // for each solve (supportIncrementalExtraction is true) then we MUST
  // update the constraint here, otherwise we lose this update information.

  if (!supportIncrementalExtraction &&
      !(slowUpdates & SlowSetConstraintBounds)) {
    InvalidateModelSynchronization();
  } else {
    if (constraint_is_extracted(index)) {
      // Constraint is already extracted, so we must update its bounds
      // and its type.
      DCHECK(mLp != nullptr);
      char sense;
      double range, rhs;
      MakeRhs(lb, ub, rhs, sense, range);
      if (sense == 'R') {
        // Rather than doing the complicated analysis required for
        // XPRSchgrhsrange(), we first convert the row into an 'L' row
        // with defined rhs and then change the range value.
        CHECK_STATUS(XPRSchgrowtype(mLp, 1, &index, "L"));
        CHECK_STATUS(XPRSchgrhs(mLp, 1, &index, &rhs));
        CHECK_STATUS(XPRSchgrhsrange(mLp, 1, &index, &range));
      } else {
        CHECK_STATUS(XPRSchgrowtype(mLp, 1, &index, &sense));
        CHECK_STATUS(XPRSchgrhs(mLp, 1, &index, &rhs));
      }
    } else {
      // Constraint is not yet extracted. It is sufficient to mark the
      // modeling object as "out of sync"
      InvalidateModelSynchronization();
    }
  }
}

void XpressInterface::AddRowConstraint(MPConstraint* const ct) {
  // This is currently only invoked when a new constraint is created,
  // see MPSolver::MakeRowConstraint().
  // At this point we only have the lower and upper bounds of the
  // constraint. We could immediately call XPRSaddrows() here but it is
  // usually much faster to handle the fully populated constraint in
  // ExtractNewConstraints() right before the solve.

  // TODO
  // Make new constraints basic (rowstat[jrow]=1)
  // Try not to delete basic variables, or non-basic constraints.
  InvalidateModelSynchronization();
}

void XpressInterface::AddVariable(MPVariable* const var) {
  // This is currently only invoked when a new variable is created,
  // see MPSolver::MakeVar().
  // At this point the variable does not appear in any constraints or
  // the objective function. We could invoke XPRSaddcols() to immediately
  // create the variable here, but it is usually much faster to handle the
  // fully set-up variable in ExtractNewVariables() right before the solve.

  // TODO
  // Make new variables non-basic at their lower bound (colstat[icol]=0), unless
  // a variable has an infinite lower bound and a finite upper bound, in which
  // case make the variable non-basic at its upper bound (colstat[icol]=2) Try
  // not to delete basic variables, or non-basic constraints.
  InvalidateModelSynchronization();
}

void XpressInterface::SetCoefficient(MPConstraint* const constraint,
                                     MPVariable const* const variable,
                                     double new_value, double) {
  InvalidateSolutionSynchronization();

  // Changing a single coefficient in the matrix is potentially pretty
  // slow since that coefficient has to be found in the sparse matrix
  // representation. So by default we don't perform this update immediately
  // but instead mark the low-level modeling object "out of sync".
  // If we want to support incremental extraction then we MUST perform
  // the modification immediately, or we will lose it.

  if (!supportIncrementalExtraction && !(slowUpdates & SlowSetCoefficient)) {
    InvalidateModelSynchronization();
  } else {
    int const row = constraint->index();
    int const col = variable->index();
    if (constraint_is_extracted(row) && variable_is_extracted(col)) {
      // If row and column are both extracted then we can directly
      // update the modeling object
      DCHECK_LE(row, last_constraint_index_);
      DCHECK_LE(col, last_variable_index_);
      CHECK_STATUS(XPRSchgcoef(mLp, row, col, new_value));
    } else {
      // If either row or column is not yet extracted then we can
      // defer the update to ExtractModel()
      InvalidateModelSynchronization();
    }
  }
}

void XpressInterface::ClearConstraint(MPConstraint* const constraint) {
  int const row = constraint->index();
  if (!constraint_is_extracted(row))
    // There is nothing to do if the constraint was not even extracted.
    return;

  // Clearing a constraint means setting all coefficients in the corresponding
  // row to 0 (we cannot just delete the row since that would renumber all
  // the constraints/rows after it).
  // Modifying coefficients in the matrix is potentially pretty expensive
  // since they must be found in the sparse matrix representation. That is
  // why by default we do not modify the coefficients here but only mark
  // the low-level modeling object "out of sync".

  if (!(slowUpdates & SlowClearConstraint)) {
    InvalidateModelSynchronization();
  } else {
    InvalidateSolutionSynchronization();

    int const len = constraint->coefficients_.size();
    unique_ptr<int[]> rowind(new int[len]);
    unique_ptr<int[]> colind(new int[len]);
    unique_ptr<double[]> val(new double[len]);
    int j = 0;
    const auto& coeffs = constraint->coefficients_;
    for (auto coeff : coeffs) {
      int const col = coeff.first->index();
      if (variable_is_extracted(col)) {
        rowind[j] = row;
        colind[j] = col;
        val[j] = 0.0;
        ++j;
      }
    }
    if (j)
      CHECK_STATUS(XPRSchgmcoef(mLp, j, rowind.get(), colind.get(), val.get()));
  }
}

void XpressInterface::SetObjectiveCoefficient(MPVariable const* const variable,
                                              double coefficient) {
  int const col = variable->index();
  if (!variable_is_extracted(col))
    // Nothing to do if variable was not even extracted
    return;

  InvalidateSolutionSynchronization();

  // The objective function is stored as a dense vector, so updating a
  // single coefficient is O(1). So by default we update the low-level
  // modeling object here.
  // If we support incremental extraction then we have no choice but to
  // perform the update immediately.

  if (supportIncrementalExtraction ||
      (slowUpdates & SlowSetObjectiveCoefficient)) {
    CHECK_STATUS(XPRSchgobj(mLp, 1, &col, &coefficient));
  } else {
    InvalidateModelSynchronization();
  }
}

void XpressInterface::SetObjectiveOffset(double value) {
  // Changing the objective offset is O(1), so we always do it immediately.
  InvalidateSolutionSynchronization();
  CHECK_STATUS(setobjoffset(mLp, value));
}

void XpressInterface::ClearObjective() {
  InvalidateSolutionSynchronization();

  // Since the objective function is stored as a dense vector updating
  // it is O(n), so we usually perform the update immediately.
  // If we want to support incremental extraction then we have no choice
  // but to perform the update immediately.

  if (supportIncrementalExtraction || (slowUpdates & SlowClearObjective)) {
    int const cols = getnumcols(mLp);
    unique_ptr<int[]> ind(new int[cols]);
    unique_ptr<double[]> zero(new double[cols]);
    int j = 0;
    const auto& coeffs = solver_->objective_->coefficients_;
    for (auto coeff : coeffs) {
      int const idx = coeff.first->index();
      // We only need to reset variables that have been extracted.
      if (variable_is_extracted(idx)) {
        DCHECK_LT(idx, cols);
        ind[j] = idx;
        zero[j] = 0.0;
        ++j;
      }
    }
    if (j > 0) {
      CHECK_STATUS(XPRSchgobj(mLp, j, ind.get(), zero.get()));
    }
    CHECK_STATUS(setobjoffset(mLp, 0.0));
  } else {
    InvalidateModelSynchronization();
  }
}

// ------ Query statistics on the solution and the solve ------

int64_t XpressInterface::iterations() const {
  if (!CheckSolutionIsSynchronized()) return kUnknownNumberOfIterations;
  return static_cast<int64_t>(getitcnt(mLp));
}

int64_t XpressInterface::nodes() const {
  if (mMip) {
    if (!CheckSolutionIsSynchronized()) return kUnknownNumberOfNodes;
    return static_cast<int64_t>(getnodecnt(mLp));
  } else {
    LOG(DFATAL) << "Number of nodes only available for discrete problems";
    return kUnknownNumberOfNodes;
  }
}

// Transform a XPRESS basis status to an MPSolver basis status.
static MPSolver::BasisStatus XpressToMPSolverBasisStatus(
    int xpress_basis_status) {
  switch (xpress_basis_status) {
    case XPRS_AT_LOWER:
      return MPSolver::AT_LOWER_BOUND;
    case XPRS_BASIC:
      return MPSolver::BASIC;
    case XPRS_AT_UPPER:
      return MPSolver::AT_UPPER_BOUND;
    case XPRS_FREE_SUPER:
      return MPSolver::FREE;
    default:
      LOG(DFATAL) << "Unknown XPRESS basis status";
      return MPSolver::FREE;
  }
}

static int MPSolverToXpressBasisStatus(
    MPSolver::BasisStatus mpsolver_basis_status) {
  switch (mpsolver_basis_status) {
    case MPSolver::AT_LOWER_BOUND:
      return XPRS_AT_LOWER;
    case MPSolver::BASIC:
      return XPRS_BASIC;
    case MPSolver::AT_UPPER_BOUND:
      return XPRS_AT_UPPER;
    case MPSolver::FREE:
      return XPRS_FREE_SUPER;
    case MPSolver::FIXED_VALUE:
      return XPRS_BASIC;
    default:
      LOG(DFATAL) << "Unknown MPSolver basis status";
      return XPRS_FREE_SUPER;
  }
}

// Returns the basis status of a row.
MPSolver::BasisStatus XpressInterface::row_status(int constraint_index) const {
  if (mMip) {
    LOG(FATAL) << "Basis status only available for continuous problems";
    return MPSolver::FREE;
  }

  if (CheckSolutionIsSynchronized()) {
    if (mRstat.empty()) {
      int const rows = getnumrows(mLp);
      mRstat.resize(rows);
      CHECK_STATUS(XPRSgetbasis(mLp, mRstat.data(), 0));
    }
  } else {
    mRstat.clear();
  }

  if (!mRstat.empty()) {
    return XpressToMPSolverBasisStatus(mRstat[constraint_index]);
  } else {
    LOG(FATAL) << "Row basis status not available";
    return MPSolver::FREE;
  }
}

// Returns the basis status of a column.
MPSolver::BasisStatus XpressInterface::column_status(int variable_index) const {
  if (mMip) {
    LOG(FATAL) << "Basis status only available for continuous problems";
    return MPSolver::FREE;
  }

  if (CheckSolutionIsSynchronized()) {
    if (mCstat.empty()) {
      int const cols = getnumcols(mLp);
      mCstat.resize(cols);
      CHECK_STATUS(XPRSgetbasis(mLp, 0, mCstat.data()));
    }
  } else {
    mCstat.clear();
  }

  if (!mCstat.empty()) {
    return XpressToMPSolverBasisStatus(mCstat[variable_index]);
  } else {
    LOG(FATAL) << "Column basis status not available";
    return MPSolver::FREE;
  }
}

// Extract all variables that have not yet been extracted.
void XpressInterface::ExtractNewVariables() {
  // NOTE: The code assumes that a linear expression can never contain
  //       non-zero duplicates.

  InvalidateSolutionSynchronization();

  if (!supportIncrementalExtraction) {
    // Without incremental extraction ExtractModel() is always called
    // to extract the full model.
    CHECK(last_variable_index_ == 0 ||
          last_variable_index_ == solver_->variables_.size());
    CHECK(last_constraint_index_ == 0 ||
          last_constraint_index_ == solver_->constraints_.size());
  }

  int const last_extracted = last_variable_index_;
  int const var_count = solver_->variables_.size();
  int new_col_count = var_count - last_extracted;
  if (new_col_count > 0) {
    // There are non-extracted variables. Extract them now.

    unique_ptr<double[]> obj(new double[new_col_count]);
    unique_ptr<double[]> lb(new double[new_col_count]);
    unique_ptr<double[]> ub(new double[new_col_count]);
    unique_ptr<char[]> ctype(new char[new_col_count]);

    for (int j = 0, var_idx = last_extracted; j < new_col_count;
         ++j, ++var_idx) {
      MPVariable const* const var = solver_->variables_[var_idx];
      lb[j] = var->lb();
      ub[j] = var->ub();
      ctype[j] = var->integer() ? XPRS_INTEGER : XPRS_CONTINUOUS;
      obj[j] = solver_->objective_->GetCoefficient(var);
    }

    // Arrays for modifying the problem are setup. Update the index
    // of variables that will get extracted now. Updating indices
    // _before_ the actual extraction makes things much simpler in
    // case we support incremental extraction.
    // In case of error we just reset the indices.
    std::vector<MPVariable*> const& variables = solver_->variables();
    for (int j = last_extracted; j < var_count; ++j) {
      CHECK(!variable_is_extracted(variables[j]->index()));
      set_variable_as_extracted(variables[j]->index(), true);
    }

    try {
      bool use_new_cols = true;

      if (supportIncrementalExtraction) {
        // If we support incremental extraction then we must
        // update existing constraints with the new variables.
        // To do that we use XPRSaddcols() to actually create the
        // variables. This is supposed to be faster than combining
        // XPRSnewcols() and XPRSchgcoeflist().

        // For each column count the size of the intersection with
        // existing constraints.
        unique_ptr<int[]> collen(new int[new_col_count]);
        for (int j = 0; j < new_col_count; ++j) collen[j] = 0;
        int nonzeros = 0;
        // TODO: Use a bitarray to flag the constraints that actually
        //       intersect new variables?
        for (int i = 0; i < last_constraint_index_; ++i) {
          MPConstraint const* const ct = solver_->constraints_[i];
          CHECK(constraint_is_extracted(ct->index()));
          const auto& coeffs = ct->coefficients_;
          for (auto coeff : coeffs) {
            int const idx = coeff.first->index();
            if (variable_is_extracted(idx) && idx > last_variable_index_) {
              collen[idx - last_variable_index_]++;
              ++nonzeros;
            }
          }
        }

        if (nonzeros > 0) {
          // At least one of the new variables did intersect with an
          // old constraint. We have to create the new columns via
          // XPRSaddcols().
          use_new_cols = false;
          unique_ptr<int[]> begin(new int[new_col_count + 2]);
          unique_ptr<int[]> cmatind(new int[nonzeros]);
          unique_ptr<double[]> cmatval(new double[nonzeros]);

          // Here is how cmatbeg[] is setup:
          // - it is initialized as
          //     [ 0, 0, collen[0], collen[0]+collen[1], ... ]
          //   so that cmatbeg[j+1] tells us where in cmatind[] and
          //   cmatval[] we need to put the next nonzero for column
          //   j
          // - after nonzeros have been set up, the array looks like
          //     [ 0, collen[0], collen[0]+collen[1], ... ]
          //   so that it is the correct input argument for XPRSaddcols
          int* cmatbeg = begin.get();
          cmatbeg[0] = 0;
          cmatbeg[1] = 0;
          ++cmatbeg;
          for (int j = 0; j < new_col_count; ++j)
            cmatbeg[j + 1] = cmatbeg[j] + collen[j];

          for (int i = 0; i < last_constraint_index_; ++i) {
            MPConstraint const* const ct = solver_->constraints_[i];
            int const row = ct->index();
            const auto& coeffs = ct->coefficients_;
            for (auto coeff : coeffs) {
              int const idx = coeff.first->index();
              if (variable_is_extracted(idx) && idx > last_variable_index_) {
                int const nz = cmatbeg[idx]++;
                cmatind[nz] = row;
                cmatval[nz] = coeff.second;
              }
            }
          }
          --cmatbeg;
          CHECK_STATUS(XPRSaddcols(mLp, new_col_count, nonzeros, obj.get(),
                                   cmatbeg, cmatind.get(), cmatval.get(),
                                   lb.get(), ub.get()));
        }
      }

      if (use_new_cols) {
        // Either incremental extraction is not supported or none of
        // the new variables did intersect an existing constraint.
        // We can just use XPRSnewcols() to create the new variables.
        std::vector<int> collen(new_col_count, 0);
        std::vector<int> cmatbeg(new_col_count, 0);
        unique_ptr<int[]> cmatind(new int[1]);
        unique_ptr<double[]> cmatval(new double[1]);
        cmatind[0] = 0;
        cmatval[0] = 1.0;

        CHECK_STATUS(XPRSaddcols(mLp, new_col_count, 0, obj.get(),
                                 cmatbeg.data(), cmatind.get(), cmatval.get(),
                                 lb.get(), ub.get()));

        int const cols = getnumcols(mLp);
        unique_ptr<int[]> ind(new int[new_col_count]);
        for (int j = 0; j < cols; ++j) ind[j] = j;
        CHECK_STATUS(
            XPRSchgcoltype(mLp, cols - last_extracted, ind.get(), ctype.get()));

      } else {
        // Incremental extraction: we must update the ctype of the
        // newly created variables (XPRSaddcols() does not allow
        // specifying the ctype)
        if (mMip && getnumcols(mLp) > 0) {
          // Query the actual number of columns in case we did not
          // manage to extract all columns.
          int const cols = getnumcols(mLp);
          unique_ptr<int[]> ind(new int[new_col_count]);
          for (int j = last_extracted; j < cols; ++j)
            ind[j - last_extracted] = j;
          CHECK_STATUS(XPRSchgcoltype(mLp, cols - last_extracted, ind.get(),
                                      ctype.get()));
        }
      }
    } catch (...) {
      // Undo all changes in case of error.
      int const cols = getnumcols(mLp);
      if (cols > last_extracted) {
        std::vector<int> cols_to_delete;
        for (int i = last_extracted; i < cols; ++i) cols_to_delete.push_back(i);
        (void)XPRSdelcols(mLp, cols_to_delete.size(), cols_to_delete.data());
      }
      std::vector<MPVariable*> const& variables = solver_->variables();
      int const size = variables.size();
      for (int j = last_extracted; j < size; ++j)
        set_variable_as_extracted(j, false);
      throw;
    }
  }
}

// Extract constraints that have not yet been extracted.
void XpressInterface::ExtractNewConstraints() {
  // NOTE: The code assumes that a linear expression can never contain
  //       non-zero duplicates.
  if (!supportIncrementalExtraction) {
    // Without incremental extraction ExtractModel() is always called
    // to extract the full model.
    CHECK(last_variable_index_ == 0 ||
          last_variable_index_ == solver_->variables_.size());
    CHECK(last_constraint_index_ == 0 ||
          last_constraint_index_ == solver_->constraints_.size());
  }

  int const offset = last_constraint_index_;
  int const total = solver_->constraints_.size();

  if (total > offset) {
    // There are constraints that are not yet extracted.

    InvalidateSolutionSynchronization();

    int newCons = total - offset;
    int const cols = getnumcols(mLp);
    int const chunk = newCons;  // 10;  // max number of rows to add in one shot

    // Update indices of new constraints _before_ actually extracting
    // them. In case of error we will just reset the indices.
    for (int c = offset; c < total; ++c) set_constraint_as_extracted(c, true);

    try {
      unique_ptr<int[]> rmatind(new int[cols]);
      unique_ptr<double[]> rmatval(new double[cols]);
      unique_ptr<int[]> rmatbeg(new int[chunk]);
      unique_ptr<char[]> sense(new char[chunk]);
      unique_ptr<double[]> rhs(new double[chunk]);
      unique_ptr<double[]> rngval(new double[chunk]);

      // Loop over the new constraints, collecting rows for up to
      // CHUNK constraints into the arrays so that adding constraints
      // is faster.
      for (int c = 0; c < newCons; /* nothing */) {
        // Collect up to CHUNK constraints into the arrays.
        int nextRow = 0;
        int nextNz = 0;
        for (/* nothing */; c < newCons && nextRow < chunk; ++c, ++nextRow) {
          MPConstraint const* const ct = solver_->constraints_[offset + c];

          // Stop if there is not enough room in the arrays
          // to add the current constraint.
          if (nextNz + ct->coefficients_.size() > cols) {
            DCHECK_GT(nextRow, 0);
            break;
          }

          // Setup right-hand side of constraint.
          MakeRhs(ct->lb(), ct->ub(), rhs[nextRow], sense[nextRow],
                  rngval[nextRow]);

          // Setup left-hand side of constraint.
          rmatbeg[nextRow] = nextNz;
          const auto& coeffs = ct->coefficients_;
          for (auto coeff : coeffs) {
            int const idx = coeff.first->index();
            if (variable_is_extracted(idx)) {
              DCHECK_LT(nextNz, cols);
              DCHECK_LT(idx, cols);
              rmatind[nextNz] = idx;
              rmatval[nextNz] = coeff.second;
              ++nextNz;
            }
          }
        }
        if (nextRow > 0) {
          CHECK_STATUS(XPRSaddrows(mLp, nextRow, nextNz, sense.get(), rhs.get(),
                                   rngval.get(), rmatbeg.get(), rmatind.get(),
                                   rmatval.get()));
        }
      }
    } catch (...) {
      // Undo all changes in case of error.
      int const rows = getnumrows(mLp);
      std::vector<int> rows_to_delete;
      for (int i = offset; i < rows; ++i) rows_to_delete.push_back(i);
      if (rows > offset)
        (void)XPRSdelrows(mLp, rows_to_delete.size(), rows_to_delete.data());
      std::vector<MPConstraint*> const& constraints = solver_->constraints();
      int const size = constraints.size();
      for (int i = offset; i < size; ++i) set_constraint_as_extracted(i, false);
      throw;
    }
  }
}

// Extract the objective function.
void XpressInterface::ExtractObjective() {
  // NOTE: The code assumes that the objective expression does not contain
  //       any non-zero duplicates.

  int const cols = getnumcols(mLp);
  // DCHECK_EQ(last_variable_index_, cols);

  unique_ptr<int[]> ind(new int[cols]);
  unique_ptr<double[]> val(new double[cols]);
  for (int j = 0; j < cols; ++j) {
    ind[j] = j;
    val[j] = 0.0;
  }

  const auto& coeffs = solver_->objective_->coefficients_;
  for (auto coeff : coeffs) {
    int const idx = coeff.first->index();
    if (variable_is_extracted(idx)) {
      DCHECK_LT(idx, cols);
      val[idx] = coeff.second;
    }
  }

  CHECK_STATUS(XPRSchgobj(mLp, cols, ind.get(), val.get()));
  CHECK_STATUS(setobjoffset(mLp, solver_->Objective().offset()));
}

// ------ Parameters  -----

void XpressInterface::SetParameters(const MPSolverParameters& param) {
  SetCommonParameters(param);
  SetScalingMode(param.GetIntegerParam(MPSolverParameters::SCALING));
  if (mMip) SetMIPParameters(param);
}

void XpressInterface::SetRelativeMipGap(double value) {
  if (mMip) {
    CHECK_STATUS(XPRSsetdblcontrol(mLp, XPRS_MIPRELSTOP, value));
  } else {
    LOG(WARNING) << "The relative MIP gap is only available "
                 << "for discrete problems.";
  }
}

void XpressInterface::SetPrimalTolerance(double value) {
  CHECK_STATUS(XPRSsetdblcontrol(mLp, XPRS_FEASTOL, value));
}

void XpressInterface::SetDualTolerance(double value) {
  CHECK_STATUS(XPRSsetdblcontrol(mLp, XPRS_OPTIMALITYTOL, value));
}

void XpressInterface::SetPresolveMode(int value) {
  auto const presolve = static_cast<MPSolverParameters::PresolveValues>(value);

  switch (presolve) {
    case MPSolverParameters::PRESOLVE_OFF:
      CHECK_STATUS(XPRSsetintcontrol(mLp, XPRS_PRESOLVE, 0));
      return;
    case MPSolverParameters::PRESOLVE_ON:
      CHECK_STATUS(XPRSsetintcontrol(mLp, XPRS_PRESOLVE, 1));
      return;
  }
  SetIntegerParamToUnsupportedValue(MPSolverParameters::PRESOLVE, value);
}

// Sets the scaling mode.
void XpressInterface::SetScalingMode(int value) {
  auto const scaling = static_cast<MPSolverParameters::ScalingValues>(value);

  switch (scaling) {
    case MPSolverParameters::SCALING_OFF:
      CHECK_STATUS(XPRSsetintcontrol(mLp, XPRS_SCALING, 0));
      break;
    case MPSolverParameters::SCALING_ON:
      CHECK_STATUS(XPRSsetdefaultcontrol(mLp, XPRS_SCALING));
      // In Xpress, scaling is not  a binary on/off control, but a bit vector
      // control setting it to 1 would only enable bit 1. Instead, we reset it
      // to its default (163 for the current version 8.6) Alternatively, we
      // could call CHECK_STATUS(XPRSsetintcontrol(mLp, XPRS_SCALING, 163));
      break;
  }
}

// Sets the LP algorithm : primal, dual or barrier. Note that XPRESS offers
// other LP algorithm (e.g. network) and automatic selection
void XpressInterface::SetLpAlgorithm(int value) {
  auto const algorithm =
      static_cast<MPSolverParameters::LpAlgorithmValues>(value);

  int alg = 1;

  switch (algorithm) {
    case MPSolverParameters::DUAL:
      alg = 2;
      break;
    case MPSolverParameters::PRIMAL:
      alg = 3;
      break;
    case MPSolverParameters::BARRIER:
      alg = 4;
      break;
  }

  if (alg == XPRS_DEFAULTALG) {
    SetIntegerParamToUnsupportedValue(MPSolverParameters::LP_ALGORITHM, value);
  } else {
    CHECK_STATUS(XPRSsetintcontrol(mLp, XPRS_DEFAULTALG, alg));
  }
}
std::vector<int> XpressBasisStatusesFrom(
    const std::vector<MPSolver::BasisStatus>& statuses) {
  std::vector<int> result;
  result.resize(statuses.size());
  std::transform(statuses.cbegin(), statuses.cend(), result.begin(),
                 MPSolverToXpressBasisStatus);
  return result;
}

void XpressInterface::SetStartingLpBasis(
    const std::vector<MPSolver::BasisStatus>& variable_statuses,
    const std::vector<MPSolver::BasisStatus>& constraint_statuses) {
  if (mMip) {
    LOG(DFATAL) << __FUNCTION__ << " is only available for LP problems";
    return;
  }
  initial_variables_basis_status_ = XpressBasisStatusesFrom(variable_statuses);
  initial_constraint_basis_status_ =
      XpressBasisStatusesFrom(constraint_statuses);
}

bool XpressInterface::readParameters(std::istream& is, char sep) {
  // - parameters must be specified as NAME=VALUE
  // - settings must be separated by sep
  // - any whitespace is ignored
  // - string parameters are not supported

  std::string name(""), value("");
  bool inValue = false;

  while (is) {
    int c = is.get();
    if (is.eof()) break;
    if (c == '=') {
      if (inValue) {
        LOG(DFATAL) << "Failed to parse parameters in " << SolverVersion();
        return false;
      }
      inValue = true;
    } else if (c == sep) {
      // End of parameter setting
      if (name.size() == 0 && value.size() == 0) {
        // Ok to have empty "lines".
        continue;
      } else if (name.size() == 0) {
        LOG(DFATAL) << "Parameter setting without name in " << SolverVersion();
      } else if (!readParameter(mLp, name, value))
        return false;

      // Reset for parsing the next parameter setting.
      name = "";
      value = "";
      inValue = false;
    } else if (std::isspace(c)) {
      continue;
    } else if (inValue) {
      value += (char)c;
    } else {
      name += (char)c;
    }
  }
  if (inValue) return readParameter(mLp, name, value);

  return true;
}

bool XpressInterface::ReadParameterFile(std::string const& filename) {
  // Return true on success and false on error.
  std::ifstream s(filename);
  if (!s) return false;
  return readParameters(s, '\n');
}

std::string XpressInterface::ValidFileExtensionForParameterFile() const {
  return ".prm";
}

MPSolver::ResultStatus XpressInterface::Solve(MPSolverParameters const& param) {
  int status;

  // Delete cached information
  mCstat.clear();
  mRstat.clear();

  WallTimer timer;
  timer.Start();

  // Set incrementality
  auto const inc = static_cast<MPSolverParameters::IncrementalityValues>(
      param.GetIntegerParam(MPSolverParameters::INCREMENTALITY));
  switch (inc) {
    case MPSolverParameters::INCREMENTALITY_OFF: {
      Reset();  // This should not be required but re-extracting everything
                // may be faster, so we do it.
      break;
    }
    case MPSolverParameters::INCREMENTALITY_ON: {
      XPRSsetintcontrol(mLp, XPRS_CRASH, 0);
      break;
    }
  }

  // Extract the model to be solved.
  // If we don't support incremental extraction and the low-level modeling
  // is out of sync then we have to re-extract everything. Note that this
  // will lose MIP starts or advanced basis information from a previous
  // solve.
  if (!supportIncrementalExtraction && sync_status_ == MUST_RELOAD) Reset();
  ExtractModel();
  VLOG(1) << absl::StrFormat("Model build in %.3f seconds.", timer.Get());

  // Set log level.
  XPRSsetintcontrol(mLp, XPRS_OUTPUTLOG, quiet() ? 0 : 1);
  // Set parameters.
  // We first set our internal MPSolverParameters from 'param' and then set
  // any user-specified internal solver parameters via
  // solver_specific_parameter_string_.
  // Default MPSolverParameters can override custom parameters while specific
  // parameters allow a higher level of customization (for example for
  // presolving) and therefore we apply MPSolverParameters first.
  SetParameters(param);
  solver_->SetSolverSpecificParametersAsString(
      solver_->solver_specific_parameter_string_);
  if (solver_->time_limit()) {
    VLOG(1) << "Setting time limit = " << solver_->time_limit() << " ms.";
    // In Xpress, a time limit should usually have a negative sign. With a
    // positive sign, the solver will only stop when a solution has been found.
    CHECK_STATUS(XPRSsetintcontrol(mLp, XPRS_MAXTIME,
                                   -1 * solver_->time_limit_in_secs()));
  }

  // Load basis if present
  // TODO : check number of variables / constraints
  if (!mMip && !initial_variables_basis_status_.empty() &&
      !initial_constraint_basis_status_.empty()) {
    CHECK_STATUS(XPRSloadbasis(mLp, initial_constraint_basis_status_.data(),
                               initial_variables_basis_status_.data()));
  }

  // Set the hint (if any)
  this->AddSolutionHintToOptimizer();

  // Add opt node callback to optimizer. We have to do this here (just before
  // solve) to make sure the variables are fully initialized
  MPCallbackWrapper* mp_callback_wrapper = nullptr;
  if (callback_ != nullptr) {
    mp_callback_wrapper = new MPCallbackWrapper(callback_);
    CHECK_STATUS(XPRSaddcbintsol(mLp, XpressIntSolCallbackImpl,
                                 static_cast<void*>(mp_callback_wrapper), 0));
  }

  // Solve.
  // Do not CHECK_STATUS here since some errors (for example CPXERR_NO_MEMORY)
  // still allow us to query useful information.
  timer.Restart();

  int xpress_stat = 0;
  if (mMip) {
    status = XPRSmipoptimize(mLp, "");
    XPRSgetintattrib(mLp, XPRS_MIPSTATUS, &xpress_stat);
  } else {
    status = XPRSlpoptimize(mLp, "");
    XPRSgetintattrib(mLp, XPRS_LPSTATUS, &xpress_stat);
  }

  if (mp_callback_wrapper != nullptr) {
    mp_callback_wrapper->LogCaughtExceptions();
    delete mp_callback_wrapper;
  }

  if (!(mMip ? (xpress_stat == XPRS_MIP_OPTIMAL)
             : (xpress_stat == XPRS_LP_OPTIMAL))) {
    XPRSpostsolve(mLp);
  }

  // Disable screen output right after solve
  XPRSsetintcontrol(mLp, XPRS_OUTPUTLOG, 0);

  if (status) {
    VLOG(1) << absl::StrFormat("Failed to optimize MIP. Error %d", status);
    // NOTE: We do not return immediately since there may be information
    //       to grab (for example an incumbent)
  } else {
    VLOG(1) << absl::StrFormat("Solved in %.3f seconds.", timer.Get());
  }

  VLOG(1) << absl::StrFormat("XPRESS solution status %d.", xpress_stat);

  // Figure out what solution we have.
  bool const feasible = (mMip ? (xpress_stat == XPRS_MIP_OPTIMAL ||
                                 xpress_stat == XPRS_MIP_SOLUTION)
                              : (!mMip && xpress_stat == XPRS_LP_OPTIMAL));

  // Get problem dimensions for solution queries below.
  int const rows = getnumrows(mLp);
  int const cols = getnumcols(mLp);
  DCHECK_EQ(rows, solver_->constraints_.size());
  DCHECK_EQ(cols, solver_->variables_.size());

  // Capture objective function value.
  objective_value_ = XPRS_NAN;
  best_objective_bound_ = XPRS_NAN;
  if (feasible) {
    if (mMip) {
      CHECK_STATUS(XPRSgetdblattrib(mLp, XPRS_MIPOBJVAL, &objective_value_));
      CHECK_STATUS(
          XPRSgetdblattrib(mLp, XPRS_BESTBOUND, &best_objective_bound_));
    } else {
      CHECK_STATUS(XPRSgetdblattrib(mLp, XPRS_LPOBJVAL, &objective_value_));
    }
  }
  VLOG(1) << "objective=" << objective_value_
          << ", bound=" << best_objective_bound_;

  // Capture primal and dual solutions
  if (mMip) {
    // If there is a primal feasible solution then capture it.
    if (feasible) {
      if (cols > 0) {
        unique_ptr<double[]> x(new double[cols]);
        CHECK_STATUS(XPRSgetmipsol(mLp, x.get(), 0));
        for (int i = 0; i < solver_->variables_.size(); ++i) {
          MPVariable* const var = solver_->variables_[i];
          var->set_solution_value(x[i]);
          VLOG(3) << var->name() << ": value =" << x[i];
        }
      }
    } else {
      for (auto& variable : solver_->variables_)
        variable->set_solution_value(XPRS_NAN);
    }

    // MIP does not have duals
    for (auto& variable : solver_->variables_)
      variable->set_reduced_cost(XPRS_NAN);
    for (auto& constraint : solver_->constraints_)
      constraint->set_dual_value(XPRS_NAN);
  } else {
    // Continuous problem.
    if (cols > 0) {
      unique_ptr<double[]> x(new double[cols]);
      unique_ptr<double[]> dj(new double[cols]);
      if (feasible) CHECK_STATUS(XPRSgetlpsol(mLp, x.get(), 0, 0, dj.get()));
      for (int i = 0; i < solver_->variables_.size(); ++i) {
        MPVariable* const var = solver_->variables_[i];
        var->set_solution_value(x[i]);
        bool value = false, dual = false;

        if (feasible) {
          var->set_solution_value(x[i]);
          value = true;
        } else {
          var->set_solution_value(XPRS_NAN);
        }
        if (feasible) {
          var->set_reduced_cost(dj[i]);
          dual = true;
        } else {
          var->set_reduced_cost(XPRS_NAN);
        }
        VLOG(3) << var->name() << ":"
                << (value ? absl::StrFormat("  value = %f", x[i]) : "")
                << (dual ? absl::StrFormat("  reduced cost = %f", dj[i]) : "");
      }
    }

    if (rows > 0) {
      unique_ptr<double[]> pi(new double[rows]);
      if (feasible) {
        CHECK_STATUS(XPRSgetlpsol(mLp, 0, 0, pi.get(), 0));
      }
      for (int i = 0; i < solver_->constraints_.size(); ++i) {
        MPConstraint* const ct = solver_->constraints_[i];
        bool dual = false;
        if (feasible) {
          ct->set_dual_value(pi[i]);
          dual = true;
        } else {
          ct->set_dual_value(XPRS_NAN);
        }
        VLOG(4) << "row " << ct->index() << ":"
                << (dual ? absl::StrFormat("  dual = %f", pi[i]) : "");
      }
    }
  }

  // Map XPRESS status to more generic solution status in MPSolver
  if (mMip) {
    switch (xpress_stat) {
      case XPRS_MIP_OPTIMAL:
        result_status_ = MPSolver::OPTIMAL;
        break;
      case XPRS_MIP_INFEAS:
        result_status_ = MPSolver::INFEASIBLE;
        break;
      case XPRS_MIP_UNBOUNDED:
        result_status_ = MPSolver::UNBOUNDED;
        break;
      default:
        result_status_ = feasible ? MPSolver::FEASIBLE : MPSolver::ABNORMAL;
        break;
    }
  } else {
    switch (xpress_stat) {
      case XPRS_LP_OPTIMAL:
        result_status_ = MPSolver::OPTIMAL;
        break;
      case XPRS_LP_INFEAS:
        result_status_ = MPSolver::INFEASIBLE;
        break;
      case XPRS_LP_UNBOUNDED:
        result_status_ = MPSolver::UNBOUNDED;
        break;
      default:
        result_status_ = feasible ? MPSolver::FEASIBLE : MPSolver::ABNORMAL;
        break;
    }
  }

  sync_status_ = SOLUTION_SYNCHRONIZED;
  return result_status_;
}

namespace {
template <class T>
struct getNameFlag;

template <>
struct getNameFlag<MPVariable> {
  enum { value = XPRS_NAMES_COLUMN };
};

template <>
struct getNameFlag<MPConstraint> {
  enum { value = XPRS_NAMES_ROW };
};

template <class T>
// T = MPVariable | MPConstraint
// or any class that has a public method name() const
void ExtractNames(XPRSprob mLp, const std::vector<T*>& objects) {
  const bool have_names =
      std::any_of(objects.begin(), objects.end(),
                  [](const T* x) { return !x->name().empty(); });

  // FICO XPRESS requires a single large const char* such as
  // "name1\0name2\0name3"
  // See
  // https://www.fico.com/fico-xpress-optimization/docs/latest/solver/optimizer/HTML/XPRSaddnames.html
  if (have_names) {
    std::vector<char> all_names;
    for (const auto& x : objects) {
      const std::string& current_name = x->name();
      std::copy(current_name.begin(), current_name.end(),
                std::back_inserter(all_names));
      all_names.push_back('\0');
    }

    // Remove trailing '\0', if any
    // Note : Calling pop_back on an empty container is undefined behavior.
    if (!all_names.empty() && all_names.back() == '\0') all_names.pop_back();

    CHECK_STATUS(XPRSaddnames(mLp, getNameFlag<T>::value, all_names.data(), 0,
                              objects.size() - 1));
  }
}
}  // namespace

void XpressInterface::Write(const std::string& filename) {
  if (sync_status_ == MUST_RELOAD) {
    Reset();
  }
  ExtractModel();

  ExtractNames(mLp, solver_->variables_);
  ExtractNames(mLp, solver_->constraints_);

  VLOG(1) << "Writing Xpress MPS \"" << filename << "\".";
  const int status = XPRSwriteprob(mLp, filename.c_str(), "");
  if (status) {
    LOG(ERROR) << "Xpress: Failed to write MPS!";
  }
}

MPSolverInterface* BuildXpressInterface(bool mip, MPSolver* const solver) {
  return new XpressInterface(solver, mip);
}
// TODO useless ?
template <class Container>
void splitMyString(const std::string& str, Container& cont, char delim = ' ') {
  std::stringstream ss(str);
  std::string token;
  while (std::getline(ss, token, delim)) {
    cont.push_back(token);
  }
}

const char* stringToCharPtr(std::string& var) { return var.c_str(); }

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

#define setParamIfPossible_MACRO(target_map, setter, converter)          \
  {                                                                      \
    auto matchingParamIter = (target_map).find(paramAndValuePair.first); \
    if (matchingParamIter != (target_map).end()) {                       \
      const auto convertedValue = converter(paramAndValuePair.second);   \
      VLOG(1) << "Setting parameter " << paramAndValuePair.first         \
              << " to value " << convertedValue << std::endl;            \
      setter(mLp, matchingParamIter->second, convertedValue);            \
      continue;                                                          \
    }                                                                    \
  }

bool XpressInterface::SetSolverSpecificParametersAsString(
    const std::string& parameters) {
  if (parameters.empty()) return true;

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
    setParamIfPossible_MACRO(mapIntegerControls_, XPRSsetintcontrol, std::stoi);
    setParamIfPossible_MACRO(mapDoubleControls_, XPRSsetdblcontrol, std::stod);
    setParamIfPossible_MACRO(mapStringControls_, XPRSsetstrcontrol,
                             stringToCharPtr);
    setParamIfPossible_MACRO(mapInteger64Controls_, XPRSsetintcontrol64,
                             std::stoll);
    LOG(ERROR) << "Unknown parameter " << paramName << " : function "
               << __FUNCTION__ << std::endl;
    return false;
  }
  return true;
}

/*****************************************************************************\
* Name:         optimizermsg
* Purpose:      Display Optimizer error messages and warnings.
* Arguments:    const char *sMsg       Message string
*               int nLen               Message length
*               int nMsgLvl            Message type
* Return Value: None
\*****************************************************************************/
void XPRS_CC optimizermsg(XPRSprob prob, void* data, const char* sMsg, int nLen,
                          int nMsgLvl) {
  auto* xprs = reinterpret_cast<operations_research::XpressInterface*>(data);
  if (!xprs->quiet()) {
    switch (nMsgLvl) {
        /* Print Optimizer error messages and warnings */
      case 4: /* error */
      case 3: /* warning */
      /* Ignore other messages */
      case 2: /* dialogue */
      case 1: /* information */
        printf("%*s\n", nLen, sMsg);
        break;
        /* Exit and flush buffers */
      default:
        fflush(nullptr);
        break;
    }
  }
}

void XpressInterface::AddSolutionHintToOptimizer() {
  // Currently the XPRESS API does not handle clearing out previous hints
  const std::size_t len = solver_->solution_hint_.size();
  if (len == 0) {
    // hint is empty, nothing to do
    return;
  }
  unique_ptr<int[]> col_ind(new int[len]);
  unique_ptr<double[]> val(new double[len]);

  for (std::size_t i = 0; i < len; ++i) {
    col_ind[i] = solver_->solution_hint_[i].first->index();
    val[i] = solver_->solution_hint_[i].second;
  }
  addhint(mLp, len, val.get(), col_ind.get());
}

void XpressInterface::SetCallback(MPCallback* mp_callback) {
  if (callback_ != nullptr) {
    // replace existing callback by removing it first
    CHECK_STATUS(XPRSremovecbintsol(mLp, XpressIntSolCallbackImpl, NULL));
  }
  callback_ = mp_callback;
}

// This is the call-back called by XPRESS when it finds a new MIP solution
// NOTE(user): This function must have this exact API, because we are passing
// it to XPRESS as a callback.
void XPRS_CC XpressIntSolCallbackImpl(XPRSprob cbprob, void* cbdata) {
  auto callback_with_context = static_cast<MPCallbackWrapper*>(cbdata);
  if (callback_with_context == nullptr ||
      callback_with_context->GetCallback() == nullptr) {
    // nothing to do
    return;
  }
  try {
    std::unique_ptr<XpressMPCallbackContext> cb_context =
        std::make_unique<XpressMPCallbackContext>(
            &cbprob, MPCallbackEvent::kMipSolution, getnodecnt(cbprob));
    callback_with_context->GetCallback()->RunCallback(cb_context.get());
  } catch (std::exception&) {
    callback_with_context->CatchException(cbprob);
  }
}

bool XpressMPCallbackContext::CanQueryVariableValues() {
  return Event() == MPCallbackEvent::kMipSolution;
}

double XpressMPCallbackContext::VariableValue(const MPVariable* variable) {
  if (variable_values_.empty()) {
    int num_vars = getnumcols(*xprsprob_);
    variable_values_.resize(num_vars);
    CHECK_STATUS(XPRSgetmipsol(*xprsprob_, variable_values_.data(), 0));
  }
  return variable_values_[variable->index()];
}

double XpressMPCallbackContext::SuggestSolution(
    const absl::flat_hash_map<const MPVariable*, double>& solution) {
  // Currently the XPRESS API does not handle clearing out previous hints
  const std::size_t len = solution.size();
  if (len == 0) {
    // hint is empty, do nothing
    return NAN;
  }
  if (Event() == MPCallbackEvent::kMipSolution) {
    // Currently, XPRESS does not handle adding a new MIP solution inside the
    // "cbintsol" callback (cb for new MIP solutions that is used here)
    // So we have to prevent the user from adding a solution
    // TODO: remove this workaround when it is handled in XPRESS
    LOG(WARNING)
        << "XPRESS does not currently allow suggesting MIP solutions after "
           "a kMipSolution event. Try another call-back.";
    return NAN;
  }
  unique_ptr<int[]> colind(new int[len]);
  unique_ptr<double[]> val(new double[len]);
  int i = 0;
  for (const auto& [var, value] : solution) {
    colind[i] = var->index();
    val[i] = value;
    ++i;
  }
  addhint(*xprsprob_, len, val.get(), colind.get());

  // XPRESS doesn't guarantee if nor when it will test the suggested solution.
  // So we return NaN because we can't know the actual objective value.
  return NAN;
}

}  // namespace operations_research
