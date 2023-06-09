// Copyright 2019 RTE
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

#if defined(USE_XPRESS)

#include <algorithm>
#include <cctype>
#include <fstream>
#include <istream>
#include <limits>
#include <memory>
#include <sstream>
#include <string>

#include "absl/strings/str_format.h"
#include "ortools/base/integral_types.h"
#include "ortools/base/logging.h"
#include "ortools/base/timer.h"
#include "ortools/linear_solver/linear_solver.h"

extern "C" {
#include "xprs.h"
}

#define XPRS_INTEGER 'I'
#define XPRS_CONTINUOUS 'C'
#define STRINGIFY2(X) #X
#define STRINGIFY(X) STRINGIFY2(X)

extern "C" {
static void XPRS_CC cbmessage(XPRSprob, void* cbdata, char const* msg,
                              int msglen, int msgtype) {
  if (msgtype < 0) {
    // msgtype < 0 is a request to flush all output.
    LOG(INFO) << std::flush;
    LOG(WARNING) << std::flush;
    LOG(ERROR) << std::flush;
  } else if (msglen > 0 || msg) {  // Empty lines have msglen=0, msg!=NULL
    switch (msgtype) {
      case 1: /* info */
        LOG(INFO) << msg << std::endl;
        break;
      case 2: /* unused */
        break;
      case 3: /* warn */
        LOG(WARNING) << msg << std::endl;
        break;
      case 4: /* error */
        LOG(ERROR) << msg << std::endl;
        break;
    }
  }
}
}

namespace {
// Get the solver version for prob as string.
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

int XPRSgetnumcols(const XPRSprob& mLp) {
  int nCols = 0;
  XPRSgetintattrib(mLp, XPRS_ORIGINALCOLS, &nCols);
  return nCols;
}

int XPRSgetnumrows(const XPRSprob& mLp) {
  int nRows = 0;
  XPRSgetintattrib(mLp, XPRS_ORIGINALROWS, &nRows);
  return nRows;
}

int XPRSgetitcnt(const XPRSprob& mLp) {
  int nIters = 0;
  XPRSgetintattrib(mLp, XPRS_SIMPLEXITER, &nIters);
  return nIters;
}

int XPRSgetnodecnt(const XPRSprob& mLp) {
  int nNodes = 0;
  XPRSgetintattrib(mLp, XPRS_NODES, &nNodes);
  return nNodes;
}

int XPRSsetobjoffset(const XPRSprob& mLp, double value) {
  XPRSsetdblcontrol(mLp, XPRS_OBJRHS, value);
  return 0;
}
}  // namespace

enum XPRS_BASIS_STATUS {
  XPRS_AT_LOWER = 0,
  XPRS_BASIC = 1,
  XPRS_AT_UPPER = 2,
  XPRS_FREE_SUPER = 3
};

// In case we need to return a double but don't have a value for that
// we just return a NaN.
#if !defined(XPRS_NAN)
#define XPRS_NAN std::numeric_limits<double>::quiet_NaN()
#endif

// The argument to this macro is the invocation of a XPRS function that
// returns a status. If the function returns non-zero the macro aborts
// the program with an appropriate error message.
#define CHECK_STATUS(s)    \
  do {                     \
    int const status_ = s; \
    CHECK_EQ(0, status_);  \
  } while (0)

namespace operations_research {

using std::unique_ptr;

// For a model that is extracted to an instance of this class there is a
// 1:1 corresponence between MPVariable instances and XPRESS columns: the
// index of an extracted variable is the column index in the XPRESS model.
// Similar for instances of MPConstraint: the index of the constraint in
// the model is the row index in the XPRESS model.
class XpressInterface : public MPSolverInterface {
 public:
  // NOTE: 'mip' specifies the type of the problem (either continuous or
  //       mixed integer. This type is fixed for the lifetime of the
  //       instance. There are no dynamic changes to the model type.
  explicit XpressInterface(MPSolver* const solver, bool mip);
  ~XpressInterface();

  // Sets the optimization direction (min/max).
  virtual void SetOptimizationDirection(bool maximize);

  // ----- Solve -----
  // Solve the problem using the parameter values specified.
  virtual MPSolver::ResultStatus Solve(MPSolverParameters const& param);

  // ----- Model modifications and extraction -----
  // Resets extracted model
  virtual void Reset();

  virtual void SetVariableBounds(int var_index, double lb, double ub);
  virtual void SetVariableInteger(int var_index, bool integer);
  virtual void SetConstraintBounds(int row_index, double lb, double ub);

  virtual void AddRowConstraint(MPConstraint* const ct);
  virtual void AddVariable(MPVariable* const var);
  virtual void SetCoefficient(MPConstraint* const constraint,
                              MPVariable const* const variable,
                              double new_value, double old_value);

  // Clear a constraint from all its terms.
  virtual void ClearConstraint(MPConstraint* const constraint);
  // Change a coefficient in the linear objective
  virtual void SetObjectiveCoefficient(MPVariable const* const variable,
                                       double coefficient);
  // Change the constant term in the linear objective.
  virtual void SetObjectiveOffset(double value);
  // Clear the objective from all its terms.
  virtual void ClearObjective();

  // ------ Query statistics on the solution and the solve ------
  // Number of simplex iterations
  virtual int64_t iterations() const;
  // Number of branch-and-bound nodes. Only available for discrete problems.
  virtual int64_t nodes() const;

  // Returns the basis status of a row.
  virtual MPSolver::BasisStatus row_status(int constraint_index) const;
  // Returns the basis status of a column.
  virtual MPSolver::BasisStatus column_status(int variable_index) const;

  // ----- Misc -----

  // Query problem type.
  // Remember that problem type is a static property that is set
  // in the constructor and never changed.
  bool IsContinuous() const override { return IsLP(); }
  bool IsLP() const override { return !mMip; }
  bool IsMIP() const override { return mMip; }

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

  bool SetSolverSpecificParametersAsString(
      const std::string& parameters) override;
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
    mCstat = 0;
    mRstat = 0;
    sync_status_ = MUST_RELOAD;
  }

  // Transform XPRESS basis status to MPSolver basis status.
  static MPSolver::BasisStatus xformBasisStatus(int xpress_basis_status);

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
  // Hence we query the status only once and cache the array. This is
  // much faster in case the basis status of more than one row/column
  // is required.
  unique_ptr<int[]> mutable mCstat;
  unique_ptr<int[]> mutable mRstat;

  // Setup the right-hand side of a constraint from its lower and upper bound.
  static void MakeRhs(double lb, double ub, double& rhs, char& sense,
                      double& range);
};

/** init XPRESS environment */
int init_xpress_env(int xpress_oem_license_key = 0) {
  int code;

  const char* xpress_from_env = getenv("XPRESS");
  std::string xpresspath;

  if (xpress_from_env == nullptr) {
#if defined(XPRESS_PATH)
    std::string path(STRINGIFY(XPRESS_PATH));
    LOG(WARNING)
        << "Environment variable XPRESS undefined. Trying compile path "
        << "'" << path << "'";
#if defined(_MSC_VER)
    // need to remove the enclosing '\"' from the string itself.
    path.erase(std::remove(path.begin(), path.end(), '\"'), path.end());
    xpresspath = path + "\\bin";
#else   // _MSC_VER
    xpresspath = path + "/bin";
#endif  // _MSC_VER
#else
    LOG(WARNING)
        << "XpressInterface Error : Environment variable XPRESS undefined.\n";
    return -1;
#endif
  } else {
    xpresspath = xpress_from_env;
  }

  /** if not an OEM key */
  if (xpress_oem_license_key == 0) {
    LOG(WARNING) << "XpressInterface : Initialising xpress-MP with parameter "
                 << xpresspath << std::endl;

    code = XPRSinit(xpresspath.c_str());

    if (!code) {
      /** XPRSbanner informs about Xpress version, options and error messages */
      char banner[1000];
      XPRSgetbanner(banner);

      LOG(WARNING) << "XpressInterface : Xpress banner :\n"
                   << banner << std::endl;
      return 0;
    } else {
      char errmsg[256];
      XPRSgetlicerrmsg(errmsg, 256);

      VLOG(0) << "XpressInterface : License error : " << errmsg << std::endl;
      VLOG(0) << "XpressInterface : XPRSinit returned code : " << code << "\n";

      char banner[1000];
      XPRSgetbanner(banner);

      LOG(ERROR) << "XpressInterface : Xpress banner :\n" << banner << "\n";
      return -1;
    }
  } else {
    /** if OEM key */
    LOG(WARNING) << "XpressInterface : Initialising xpress-MP with OEM key "
                 << xpress_oem_license_key << "\n";

    int nvalue = 0;
    int ierr;
    char slicmsg[256] = "";
    char errmsg[256];

    XPRSlicense(&nvalue, slicmsg);
    VLOG(0) << "XpressInterface : First message from XPRSLicense : " << slicmsg
            << "\n";

    nvalue = xpress_oem_license_key - ((nvalue * nvalue) / 19);
    ierr = XPRSlicense(&nvalue, slicmsg);

    VLOG(0) << "XpressInterface : Second message from XPRSLicense : " << slicmsg
            << "\n";
    if (ierr == 16) {
      VLOG(0) << "XpressInterface : Optimizer development software detected\n";
    } else if (ierr != 0) {
      /** get the license error message */
      XPRSgetlicerrmsg(errmsg, 256);

      LOG(ERROR) << "XpressInterface : " << errmsg << "\n";
      return -1;
    }

    code = XPRSinit(NULL);

    if (!code) {
      return 0;
    } else {
      LOG(ERROR) << "XPRSinit returned code : " << code << "\n";
      return -1;
    }
  }
}

// Creates a LP/MIP instance.
XpressInterface::XpressInterface(MPSolver* const solver, bool mip)
    : MPSolverInterface(solver),
      mLp(0),
      mMip(mip),
      supportIncrementalExtraction(false),
      slowUpdates(static_cast<SlowUpdates>(SlowSetObjectiveCoefficient |
                                           SlowClearObjective)),
      mCstat(),
      mRstat() {
  int status = init_xpress_env();
  CHECK_STATUS(status);
  status = XPRScreateprob(&mLp);
  CHECK_STATUS(status);
  DCHECK(mLp != nullptr);  // should not be NULL if status=0
  CHECK_STATUS(XPRSloadlp(mLp, "newProb", 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0));

  CHECK_STATUS(
      XPRSchgobjsense(mLp, maximize_ ? XPRS_OBJ_MAXIMIZE : XPRS_OBJ_MINIMIZE));
}

XpressInterface::~XpressInterface() {
  CHECK_STATUS(XPRSdestroyprob(mLp));
  CHECK_STATUS(XPRSfree());
}

std::string XpressInterface::SolverVersion() const {
  return getSolverVersion(mLp);
}

// ------ Model modifications and extraction -----

void XpressInterface::Reset() {
  // Instead of explicitly clearing all modeling objects we
  // just delete the problem object and allocate a new one.
  CHECK_STATUS(XPRSdestroyprob(mLp));

  int status;
  status = XPRScreateprob(&mLp);
  CHECK_STATUS(status);
  DCHECK(mLp != nullptr);  // should not be NULL if status=0
  CHECK_STATUS(XPRSloadlp(mLp, "newProb", 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0));

  CHECK_STATUS(
      XPRSchgobjsense(mLp, maximize_ ? XPRS_OBJ_MAXIMIZE : XPRS_OBJ_MINIMIZE));

  ResetExtractionInformation();
  mCstat = 0;
  mRstat = 0;
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
  // update here or we will lose it.

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

  if (!supportIncrementalExtraction && !slowUpdates &&
      !SlowSetVariableInteger) {
    InvalidateModelSynchronization();
  } else {
    if (mMip) {
      if (variable_is_extracted(var_index)) {
        // Variable is extracted. Change the type immediately.
        // TODO: Should we check the current type and don't do anything
        //       in case the type does not change?
        DCHECK_LE(var_index, XPRSgetnumcols(mLp));
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
      LOG(DFATAL) << "XPRESS does not support contradictory bounds on range "
                     "constraints! ["
                  << lb << ", " << ub << "] will be converted to " << ub << ", "
                  << (ub - std::abs(ub - lb)) << "]";
    }
    rhs = ub;
    range = std::abs(
        ub - lb);  // This happens implicitly by XPRSaddrows() and XPRSloadlp()
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
      DCHECK(mLp != NULL);
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
  InvalidateModelSynchronization();
}

void XpressInterface::AddVariable(MPVariable* const ct) {
  // This is currently only invoked when a new variable is created,
  // see MPSolver::MakeVar().
  // At this point the variable does not appear in any constraints or
  // the objective function. We could invoke XPRSaddcols() to immediately
  // create the variable here but it is usually much faster to handle the
  // fully setup variable in ExtractNewVariables() right before the solve.
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
  // the modification immediately or we will lose it.

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
    for (auto it(coeffs.begin()); it != coeffs.end(); ++it) {
      int const col = it->first->index();
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
  CHECK_STATUS(XPRSsetobjoffset(mLp, value));
}

void XpressInterface::ClearObjective() {
  InvalidateSolutionSynchronization();

  // Since the objective function is stored as a dense vector updating
  // it is O(n), so we usually perform the update immediately.
  // If we want to support incremental extraction then we have no choice
  // but to perform the update immediately.

  if (supportIncrementalExtraction || (slowUpdates & SlowClearObjective)) {
    int const cols = XPRSgetnumcols(mLp);
    unique_ptr<int[]> ind(new int[cols]);
    unique_ptr<double[]> zero(new double[cols]);
    int j = 0;
    const auto& coeffs = solver_->objective_->coefficients_;
    for (auto it(coeffs.begin()); it != coeffs.end(); ++it) {
      int const idx = it->first->index();
      // We only need to reset variables that have been extracted.
      if (variable_is_extracted(idx)) {
        DCHECK_LT(idx, cols);
        ind[j] = idx;
        zero[j] = 0.0;
        ++j;
      }
    }
    if (j > 0) CHECK_STATUS(XPRSchgobj(mLp, j, ind.get(), zero.get()));
    CHECK_STATUS(XPRSsetobjoffset(mLp, 0.0));
  } else {
    InvalidateModelSynchronization();
  }
}

// ------ Query statistics on the solution and the solve ------

int64_t XpressInterface::iterations() const {
  if (!CheckSolutionIsSynchronized()) return kUnknownNumberOfIterations;
  return static_cast<int64_t>(XPRSgetitcnt(mLp));
}

int64_t XpressInterface::nodes() const {
  if (mMip) {
    if (!CheckSolutionIsSynchronized()) return kUnknownNumberOfNodes;
    return static_cast<int64_t>(XPRSgetnodecnt(mLp));
  } else {
    LOG(DFATAL) << "Number of nodes only available for discrete problems";
    return kUnknownNumberOfNodes;
  }
}

// Transform a XPRESS basis status to an MPSolver basis status.
MPSolver::BasisStatus XpressInterface::xformBasisStatus(
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

// Returns the basis status of a row.
MPSolver::BasisStatus XpressInterface::row_status(int constraint_index) const {
  if (mMip) {
    LOG(FATAL) << "Basis status only available for continuous problems";
    return MPSolver::FREE;
  }

  if (CheckSolutionIsSynchronized()) {
    if (!mRstat) {
      int const rows = XPRSgetnumrows(mLp);
      unique_ptr<int[]> data(new int[rows]);
      mRstat.swap(data);
      CHECK_STATUS(XPRSgetbasis(mLp, 0, mRstat.get()));
    }
  } else {
    mRstat = 0;
  }

  if (mRstat) {
    return xformBasisStatus(mRstat[constraint_index]);
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
    if (!mCstat) {
      int const cols = XPRSgetnumcols(mLp);
      unique_ptr<int[]> data(new int[cols]);
      mCstat.swap(data);
      CHECK_STATUS(XPRSgetbasis(mLp, mCstat.get(), 0));
    }
  } else {
    mCstat = 0;
  }

  if (mCstat) {
    return xformBasisStatus(mCstat[variable_index]);
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
  int newcols = var_count - last_extracted;
  if (newcols > 0) {
    // There are non-extracted variables. Extract them now.

    unique_ptr<double[]> obj(new double[newcols]);
    unique_ptr<double[]> lb(new double[newcols]);
    unique_ptr<double[]> ub(new double[newcols]);
    unique_ptr<char[]> ctype(new char[newcols]);
    unique_ptr<const char*[]> colname(new const char*[newcols]);

    bool have_names = false;
    for (int j = 0, varidx = last_extracted; j < newcols; ++j, ++varidx) {
      MPVariable const* const var = solver_->variables_[varidx];
      lb[j] = var->lb();
      ub[j] = var->ub();
      ctype[j] = var->integer() ? XPRS_INTEGER : XPRS_CONTINUOUS;
      colname[j] = var->name().empty() ? 0 : var->name().c_str();
      have_names = have_names || var->name().empty();
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
      bool use_newcols = true;

      if (supportIncrementalExtraction) {
        // If we support incremental extraction then we must
        // update existing constraints with the new variables.
        // To do that we use XPRSaddcols() to actually create the
        // variables. This is supposed to be faster than combining
        // XPRSnewcols() and XPRSchgcoeflist().

        // For each column count the size of the intersection with
        // existing constraints.
        unique_ptr<int[]> collen(new int[newcols]);
        for (int j = 0; j < newcols; ++j) collen[j] = 0;
        int nonzeros = 0;
        // TODO: Use a bitarray to flag the constraints that actually
        //       intersect new variables?
        for (int i = 0; i < last_constraint_index_; ++i) {
          MPConstraint const* const ct = solver_->constraints_[i];
          CHECK(constraint_is_extracted(ct->index()));
          const auto& coeffs = ct->coefficients_;
          for (auto it(coeffs.begin()); it != coeffs.end(); ++it) {
            int const idx = it->first->index();
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
          use_newcols = false;
          unique_ptr<int[]> begin(new int[newcols + 2]);
          unique_ptr<int[]> cmatind(new int[nonzeros]);
          unique_ptr<double[]> cmatval(new double[nonzeros]);

          // Here is how cmatbeg[] is setup:
          // - it is initialized as
          //     [ 0, 0, collen[0], collen[0]+collen[1], ... ]
          //   so that cmatbeg[j+1] tells us where in cmatind[] and
          //   cmatval[] we need to put the next nonzero for column
          //   j
          // - after nonzeros have been setup the array looks like
          //     [ 0, collen[0], collen[0]+collen[1], ... ]
          //   so that it is the correct input argument for XPRSaddcols
          int* cmatbeg = begin.get();
          cmatbeg[0] = 0;
          cmatbeg[1] = 0;
          ++cmatbeg;
          for (int j = 0; j < newcols; ++j)
            cmatbeg[j + 1] = cmatbeg[j] + collen[j];

          for (int i = 0; i < last_constraint_index_; ++i) {
            MPConstraint const* const ct = solver_->constraints_[i];
            int const row = ct->index();
            const auto& coeffs = ct->coefficients_;
            for (auto it(coeffs.begin()); it != coeffs.end(); ++it) {
              int const idx = it->first->index();
              if (variable_is_extracted(idx) && idx > last_variable_index_) {
                int const nz = cmatbeg[idx]++;
                cmatind[nz] = row;
                cmatval[nz] = it->second;
              }
            }
          }
          --cmatbeg;
          CHECK_STATUS(XPRSaddcols(mLp, newcols, nonzeros, obj.get(), cmatbeg,
                                   cmatind.get(), cmatval.get(), lb.get(),
                                   ub.get()));
        }
      }

      if (use_newcols) {
        // Either incremental extraction is not supported or none of
        // the new variables did intersect an existing constraint.
        // We can just use XPRSnewcols() to create the new variables.
        std::vector<int> collen(newcols, 0);
        std::vector<int> cmatbeg(newcols, 0);
        unique_ptr<int[]> cmatind(new int[1]);
        unique_ptr<double[]> cmatval(new double[1]);
        cmatind[0] = 0;
        cmatval[0] = 1.0;

        CHECK_STATUS(XPRSaddcols(mLp, newcols, 0, obj.get(), cmatbeg.data(),
                                 cmatind.get(), cmatval.get(), lb.get(),
                                 ub.get()));
        int const cols = XPRSgetnumcols(mLp);
        unique_ptr<int[]> ind(new int[newcols]);
        for (int j = 0; j < cols; ++j) ind[j] = j;
        CHECK_STATUS(
            XPRSchgcoltype(mLp, cols - last_extracted, ind.get(), ctype.get()));
      } else {
        // Incremental extraction: we must update the ctype of the
        // newly created variables (XPRSaddcols() does not allow
        // specifying the ctype)
        if (mMip && XPRSgetnumcols(mLp) > 0) {
          // Query the actual number of columns in case we did not
          // manage to extract all columns.
          int const cols = XPRSgetnumcols(mLp);
          unique_ptr<int[]> ind(new int[newcols]);
          for (int j = last_extracted; j < cols; ++j)
            ind[j - last_extracted] = j;
          CHECK_STATUS(XPRSchgcoltype(mLp, cols - last_extracted, ind.get(),
                                      ctype.get()));
        }
      }
    } catch (...) {
      // Undo all changes in case of error.
      int const cols = XPRSgetnumcols(mLp);
      if (cols > last_extracted) {
        std::vector<int> colsToDelete;
        for (int i = last_extracted; i < cols; ++i) colsToDelete.push_back(i);
        (void)XPRSdelcols(mLp, colsToDelete.size(), colsToDelete.data());
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
    int const cols = XPRSgetnumcols(mLp);
    DCHECK_EQ(last_variable_index_, cols);
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
      unique_ptr<char const*[]> name(new char const*[chunk]);
      unique_ptr<double[]> rngval(new double[chunk]);

      // Loop over the new constraints, collecting rows for up to
      // CHUNK constraints into the arrays so that adding constraints
      // is faster.
      for (int c = 0; c < newCons; /* nothing */) {
        // Collect up to CHUNK constraints into the arrays.
        int nextRow = 0;
        int nextNz = 0;
        bool haveRanges = false;
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
          haveRanges = haveRanges || (rngval[nextRow] != 0.0);

          // Setup left-hand side of constraint.
          rmatbeg[nextRow] = nextNz;
          const auto& coeffs = ct->coefficients_;
          for (auto it(coeffs.begin()); it != coeffs.end(); ++it) {
            int const idx = it->first->index();
            if (variable_is_extracted(idx)) {
              DCHECK_LT(nextNz, cols);
              DCHECK_LT(idx, cols);
              rmatind[nextNz] = idx;
              rmatval[nextNz] = it->second;
              ++nextNz;
            }
          }

          // Finally the name of the constraint.
          name[nextRow] = ct->name().empty() ? 0 : ct->name().c_str();
        }
        if (nextRow > 0) {
          CHECK_STATUS(XPRSaddrows(mLp, nextRow, nextNz, sense.get(), rhs.get(),
                                   haveRanges ? rngval.get() : 0, rmatbeg.get(),
                                   rmatind.get(), rmatval.get()));
        }
      }
    } catch (...) {
      // Undo all changes in case of error.
      int const rows = XPRSgetnumrows(mLp);
      std::vector<int> rowsToDelete;
      for (int i = offset; i < rows; ++i) rowsToDelete.push_back(i);
      if (rows > offset)
        (void)XPRSdelrows(mLp, rowsToDelete.size(), rowsToDelete.data());
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

  int const cols = XPRSgetnumcols(mLp);
  DCHECK_EQ(last_variable_index_, cols);

  unique_ptr<int[]> ind(new int[cols]);
  unique_ptr<double[]> val(new double[cols]);
  for (int j = 0; j < cols; ++j) {
    ind[j] = j;
    val[j] = 0.0;
  }

  const auto& coeffs = solver_->objective_->coefficients_;
  for (auto it = coeffs.begin(); it != coeffs.end(); ++it) {
    int const idx = it->first->index();
    if (variable_is_extracted(idx)) {
      DCHECK_LT(idx, cols);
      val[idx] = it->second;
    }
  }

  CHECK_STATUS(XPRSchgobj(mLp, cols, ind.get(), val.get()));
  CHECK_STATUS(XPRSsetobjoffset(mLp, solver_->Objective().offset()));
}

// ------ Parameters  -----

void XpressInterface::SetParameters(const MPSolverParameters& param) {
  SetCommonParameters(param);
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
  MPSolverParameters::PresolveValues const presolve =
      static_cast<MPSolverParameters::PresolveValues>(value);

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
  MPSolverParameters::ScalingValues const scaling =
      static_cast<MPSolverParameters::ScalingValues>(value);

  switch (scaling) {
    case MPSolverParameters::SCALING_OFF:
      CHECK_STATUS(XPRSsetintcontrol(mLp, XPRS_SCALING, 0));
      break;
    case MPSolverParameters::SCALING_ON:
      CHECK_STATUS(XPRSsetdefaultcontrol(mLp, XPRS_SCALING));
      // In Xpress, scaling is not  a binary on/off control, but a bit vector
      // control setting it to 1 would only enable bit 1. Instead we reset it to
      // its default (163 for the current version 8.6) Alternatively, we could
      // call CHECK_STATUS(XPRSsetintcontrol(mLp, XPRS_SCALING, 163));
      break;
  }
}

// Sets the LP algorithm : primal, dual or barrier. Note that XPRESS offers
// other LP algorithm (e.g. network) and automatic selection
void XpressInterface::SetLpAlgorithm(int value) {
  MPSolverParameters::LpAlgorithmValues const algorithm =
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

bool XpressInterface::SetSolverSpecificParametersAsString(
    const std::string& parameters) {
  if (parameters.empty()) {
    return true;
  }
  std::stringstream s(parameters);
  return readParameters(s, ';');
}

MPSolver::ResultStatus XpressInterface::Solve(MPSolverParameters const& param) {
  int status;

  // Delete chached information
  mCstat = 0;
  mRstat = 0;

  WallTimer timer;
  timer.Start();

  // Set incrementality
  MPSolverParameters::IncrementalityValues const inc =
      static_cast<MPSolverParameters::IncrementalityValues>(
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
  // is out of sync then we have to re-extract everything.
  if (!supportIncrementalExtraction && sync_status_ == MUST_RELOAD) Reset();
  ExtractModel();
  VLOG(1) << absl::StrFormat("Model build in %.3f seconds.", timer.Get());

  // Enable log output.
  if (!quiet()) XPRSaddcbmessage(mLp, cbmessage, nullptr, 0);
  // Set parameters.
  solver_->SetSolverSpecificParametersAsString(
      solver_->solver_specific_parameter_string_);
  SetParameters(param);
  if (solver_->time_limit()) {
    VLOG(1) << "Setting time limit = " << solver_->time_limit() << " ms.";
    // In Xpress, a time limit should usually have a negative sign. With a
    // positive sign, the solver will only stop when a solution has been found.
    CHECK_STATUS(XPRSsetintcontrol(mLp, XPRS_MAXTIME,
                                   -1.0 * solver_->time_limit_in_secs()));
  }

  timer.Restart();
  // Solve.
  // Do not CHECK_STATUS here since some errors still allow us to query useful
  // information.
  int xpressstat = 0;
  if (mMip) {
    if (this->maximize_)
      status = XPRSmaxim(mLp, "g");
    else
      status = XPRSminim(mLp, "g");
    XPRSgetintattrib(mLp, XPRS_MIPSTATUS, &xpressstat);
  } else {
    if (this->maximize_)
      status = XPRSmaxim(mLp, "");
    else
      status = XPRSminim(mLp, "");
    XPRSgetintattrib(mLp, XPRS_LPSTATUS, &xpressstat);
  }

  // Disable screen output right after solve
  XPRSremovecbmessage(mLp, cbmessage, nullptr);

  if (status) {
    VLOG(1) << absl::StrFormat("Failed to optimize MIP. Error %d", status);
    // NOTE: We do not return immediately since there may be information
    //       to grab (for example an incumbent)
  } else {
    VLOG(1) << absl::StrFormat("Solved in %.3f seconds.", timer.Get());
  }

  VLOG(1) << absl::StrFormat("XPRESS solution status %d.", xpressstat);

  // Figure out what solution we have.
  bool const feasible = (mMip && (xpressstat == XPRS_MIP_OPTIMAL ||
                                  xpressstat == XPRS_MIP_SOLUTION)) ||
                        (!mMip && xpressstat == XPRS_LP_OPTIMAL);

  // Get problem dimensions for solution queries below.
  int const rows = XPRSgetnumrows(mLp);
  int const cols = XPRSgetnumcols(mLp);
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
      for (int i = 0; i < solver_->variables_.size(); ++i)
        solver_->variables_[i]->set_solution_value(XPRS_NAN);
    }

    // MIP does not have duals
    for (int i = 0; i < solver_->variables_.size(); ++i)
      solver_->variables_[i]->set_reduced_cost(XPRS_NAN);
    for (int i = 0; i < solver_->constraints_.size(); ++i)
      solver_->constraints_[i]->set_dual_value(XPRS_NAN);
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
    switch (xpressstat) {
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
    switch (xpressstat) {
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

MPSolverInterface* BuildXpressInterface(bool mip, MPSolver* const solver) {
  return new XpressInterface(solver, mip);
}

}  // namespace operations_research
#endif  // #if defined(USE_XPRESS)
