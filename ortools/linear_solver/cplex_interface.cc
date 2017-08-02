// Copyright 2014 IBM Corporation
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

// Initial version of this code was written by Daniel Junglas (IBM)

#include <limits>
#include <memory>

#include "ortools/base/integral_types.h"
#include "ortools/base/logging.h"
#include "ortools/base/stringprintf.h"
#include "ortools/base/timer.h"
#include "ortools/linear_solver/linear_solver.h"

#if defined(USE_CPLEX)

extern "C" {
#  include "ilcplex/cplexx.h"
   // This is an undocumented function, setting the objective offset
   // is not supported everywhere (for example it may not be exported if a
   // model is written to a file), but it works in the cases we need here.
   CPXLIBAPI int CPXPUBLIC CPXEsetobjoffset(CPXCENVptr, CPXLPptr, double);
}

// In case we need to return a double but don't have a value for that
// we just return a NaN.
#if !defined(CPX_NAN)
#   define CPX_NAN std::numeric_limits<double>::quiet_NaN()
#endif

// The argument to this macro is the invocation of a CPXX function that
// returns a status. If the function returns non-zero the macro aborts
// the program with an appropriate error message.
#define CHECK_STATUS(s) do {                    \
      int const status_ = s;                    \
      CHECK_EQ(0, status_);                     \
   } while (0)

namespace operations_research {

   using std::unique_ptr;

   // For a model that is extracted to an instance of this class there is a
   // 1:1 corresponence between MPVariable instances and CPLEX columns: the
   // index of an extracted variable is the column index in the CPLEX model.
   // Similiar for instances of MPConstraint: the index of the constraint in
   // the model is the row index in the CPLEX model.
   class CplexInterface : public MPSolverInterface {
   public:
      // NOTE: 'mip' specifies the type of the problem (either continuous or
      //       mixed integer. This type is fixed for the lifetime of the
      //       instance. There are no dynamic changes to the model type.
      explicit CplexInterface(MPSolver* const solver, bool mip);
      ~CplexInterface();

      // Sets the optimization direction (min/max).
      virtual void SetOptimizationDirection(bool maximize);

      // ----- Solve -----
      // Solve the problem using the parameter values specified.
      virtual MPSolver::ResultStatus Solve(MPSolverParameters const&param);

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
      virtual int64 iterations() const;
      // Number of branch-and-bound nodes. Only available for discrete problems.
      virtual int64 nodes() const;
      // Best objective bound. Only available for discrete problems.
      virtual double best_objective_bound() const;

      // Returns the basis status of a row.
      virtual MPSolver::BasisStatus row_status(int constraint_index) const;
      // Returns the basis status of a column.
      virtual MPSolver::BasisStatus column_status(int variable_index) const;

      // ----- Misc -----

      // Query problem type.
      // Remember that problem type is a static property that is set
      // in the constructor and never changed.
      virtual bool IsContinuous() const { return IsLP(); }
      virtual bool IsLP() const { return !mMip; }
      virtual bool IsMIP() const { return mMip; }

      virtual void ExtractNewVariables();
      virtual void ExtractNewConstraints();
      virtual void ExtractObjective();

      virtual std::string SolverVersion() const;

      virtual void* underlying_solver() {
         return reinterpret_cast<void*>(mLp);
      }

      virtual double ComputeExactConditionNumber() const {
         if (!IsContinuous()) {
            LOG(DFATAL) << "ComputeExactConditionNumber not implemented for"
                        << " CPLEX_MIXED_INTEGER_PROGRAMMING";
            return CPX_NAN;
         }

         if ( CheckSolutionIsSynchronized() ) {
            double kappa = CPX_NAN;
            CHECK_STATUS(CPXXgetdblquality(mEnv, mLp, &kappa, CPX_EXACT_KAPPA));
            return kappa;
         }
         LOG(DFATAL) << "Cannot get exact condition number without solution";
         return CPX_NAN;
      }

   protected:
      // Set all parameters in the underlying solver.
      virtual void SetParameters(MPSolverParameters const &param);
      // Set each parameter in the underlying solver.
      virtual void SetRelativeMipGap(double value);
      virtual void SetPrimalTolerance(double value);
      virtual void SetDualTolerance(double value);
      virtual void SetPresolveMode(int value);
      virtual void SetScalingMode(int value);
      virtual void SetLpAlgorithm(int value);

      virtual bool ReadParameterFile(std::string const &filename);
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

      // Transform CPLEX basis status to MPSolver basis status.
      static MPSolver::BasisStatus xformBasisStatus(int cplex_basis_status);

   private:
      CPXLPptr mLp;
      CPXENVptr mEnv;
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
         SlowSetCoefficient          = 0x0001,
         SlowClearConstraint         = 0x0002,
         SlowSetObjectiveCoefficient = 0x0004,
         SlowClearObjective          = 0x0008,
         SlowSetConstraintBounds     = 0x0010,
         SlowSetVariableInteger      = 0x0020,
         SlowSetVariableBounds       = 0x0040,
         SlowUpdatesAll              = 0xffff
      } const slowUpdates;
      // CPLEX has no method to query the basis status of a single variable.
      // Hence we query the status only once and cache the array. This is
      // much faster in case the basis status of more than one row/column
      // is required.
      unique_ptr<int[]> mutable mCstat;
      unique_ptr<int[]> mutable mRstat;

      // Setup the right-hand side of a constraint from its lower and upper bound.
      static void MakeRhs(double lb, double ub,
                          double &rhs, char &sense, double &range);
   };

   // Creates a LP/MIP instance.
   CplexInterface::CplexInterface(MPSolver* const solver, bool mip)
      : MPSolverInterface(solver)
      , mEnv(0)
      , mLp(0)
      , mMip(mip)
      , slowUpdates(static_cast<SlowUpdates>(SlowSetObjectiveCoefficient
                                             | SlowClearObjective))
      , supportIncrementalExtraction(false)
      , mCstat()
      , mRstat()
   {
      int status;

      mEnv = CPXXopenCPLEX(&status);
      CHECK_STATUS(status);
      DCHECK(mEnv != nullptr); // should not be NULL if status=0

      char const *name = solver_->name_.c_str();
      mLp = CPXXcreateprob(mEnv, &status, name);
      CHECK_STATUS(status);
      DCHECK(mLp != nullptr); // should not be NULL if status=0

      CHECK_STATUS(CPXXchgobjsen(mEnv, mLp, maximize_ ? CPX_MAX : CPX_MIN));
      if ( mMip )
         CHECK_STATUS(CPXXchgprobtype(mEnv, mLp, CPXPROB_MILP));
   }

   CplexInterface::~CplexInterface()
   {
      CHECK_STATUS(CPXXfreeprob(mEnv, &mLp));
      CHECK_STATUS(CPXXcloseCPLEX(&mEnv));
   }

   std::string CplexInterface::SolverVersion() const {
      // We prefer CPXXversionnumber() over CPXXversion() since the
      // former will never pose any encoding issues.
      int version = 0;
      CHECK_STATUS(CPXXversionnumber(mEnv, &version));

      int const major   = version / 1000000; version -= major * 1000000;
      int const release = version / 10000;   version -= release * 10000;
      int const mod     = version / 100;     version -= mod * 100;
      int const fix     = version;

      return StringPrintf("CPLEX library version %d.%02d.%02d.%02d",
                          major, release, mod, fix);
   }

   // ------ Model modifications and extraction -----

   void CplexInterface::Reset()
   {
      // Instead of explicitly clearing all modeling objects we
      // just delete the problem object and allocate a new one.
      CHECK_STATUS(CPXXfreeprob(mEnv, &mLp));

      int status;
      const char* const name = solver_->name_.c_str();
      mLp = CPXXcreateprob(mEnv, &status, name);
      CHECK_STATUS(status);
      DCHECK(mLp != nullptr); // should not be NULL if status=0

      CHECK_STATUS(CPXXchgobjsen(mEnv, mLp, maximize_ ? CPX_MAX : CPX_MIN));
      if ( mMip )
         CHECK_STATUS(CPXXchgprobtype(mEnv, mLp, CPXPROB_MILP));

      ResetExtractionInformation();
      mCstat = 0;
      mRstat = 0;
   }

   void CplexInterface::SetOptimizationDirection(bool maximize)
   {
      InvalidateSolutionSynchronization();
      CPXXchgobjsen(mEnv, mLp, maximize ? CPX_MAX : CPX_MIN);
   }

   void CplexInterface::SetVariableBounds(int var_index, double lb, double ub) {
      InvalidateSolutionSynchronization();

      // Changing the bounds of a variable is fast. However, doing this for
      // many variables may still be slow. So we don't perform the update by
      // default. However, if we support incremental extraction
      // (supportIncrementalExtraction is true) then we MUST perform the
      // update here or we will lose it.

      if ( !supportIncrementalExtraction &&
           !(slowUpdates & SlowSetVariableBounds) )
      {
         InvalidateModelSynchronization();
      }
      else {
         if ( variable_is_extracted(var_index) ) {
            // Variable has already been extracted, so we must modify the
            // modeling object.
            DCHECK_LT(var_index, last_variable_index_);
            char const lu[2] = { 'L', 'U' };
            double const bd[2] = { lb, ub };
            CPXDIM const idx[2] = { var_index, var_index };
            CHECK_STATUS(CPXXchgbds(mEnv, mLp, 2, idx, lu, bd));
         }
         else {
            // Variable is not yet extracted. It is sufficient to just mark
            // the modeling object "out of sync"
            InvalidateModelSynchronization();
         }
      }
   }

   // Modifies integrality of an extracted variable.
   void CplexInterface::SetVariableInteger(int var_index, bool integer)
   {
      InvalidateSolutionSynchronization();

      // NOTE: The type of the model (continuous or mixed integer) is
      //       defined once and for all in the constructor. There are no
      //       dynamic changes to the model type.

      // Changing the type of a variable should be fast. Still, doing all
      // updates in one big chunk right before solve() is usually faster.
      // However, if we support incremental extraction
      // (supportIncrementalExtraction is true) then we MUST change the
      // type of extracted variables here.

      if ( !supportIncrementalExtraction &&
           !(slowUpdates && SlowSetVariableInteger) )
      {
         InvalidateModelSynchronization();
      }
      else {
         if ( mMip ) {
            if ( variable_is_extracted(var_index) ) {
               // Variable is extracted. Change the type immediately.
               // TODO: Should we check the current type and don't do anything
               //       in case the type does not change?
               DCHECK_LE(var_index, CPXXgetnumcols(mEnv, mLp));
               char const type = integer ? CPX_INTEGER : CPX_CONTINUOUS;
               CHECK_STATUS (CPXXchgctype(mEnv, mLp, 1, &var_index, &type));
            }
            else
               InvalidateModelSynchronization();
         }
         else {
            LOG(DFATAL)
               << "Attempt to change variable to integer in non-MIP problem!";
         }
      }
   }

   // Setup the right-hand side of a constraint.
   void CplexInterface::MakeRhs(double lb, double ub, double &rhs, char &sense, double &range)
   {
      if ( lb == ub ) {
         // Both bounds are equal -> this is an equality constraint
         rhs = lb;
         range = 0.0;
         sense = 'E';
      }
      else if ( lb > -CPX_INFBOUND && ub < CPX_INFBOUND ) {
         // Both bounds are finite -> this is a ranged constraint
         // The value of a ranged constraint is allowed to be in
         //   [ rhs[i], rhs[i]+rngval[i] ]
         // see also the reference documentation for CPXXnewrows()
         if ( ub < lb ) {
            // The bounds for the constraint are contradictory. CPLEX models
            // a range constraint l <= ax <= u as
            //    ax = l + v
            // where v is an auxiliary variable the range of which is controlled
            // by l and u: if l < u then v in [0, u-l]
            //             else          v in [u-l, 0]
            // (the range is specified as the rngval[] argument to CPXXnewrows).
            // Thus CPLEX cannot represent range constraints with contradictory
            // bounds and we must error out here.
            CHECK_STATUS(CPXERR_BAD_ARGUMENT);
         }
         rhs = lb;
         range = ub - lb;
         sense = 'R';
      }
      else if ( ub < CPX_INFBOUND ||
                (fabs(ub) == CPX_INFBOUND && fabs(lb) > CPX_INFBOUND) )
      {
         // Finite upper, infinite lower bound -> this is a <= constraint
         rhs = ub;
         range = 0.0;
         sense = 'L';
      }
      else if ( lb > -CPX_INFBOUND ||
                (fabs(lb) == CPX_INFBOUND && fabs(ub) > CPX_INFBOUND) )
      {
         // Finite lower, infinite upper bound -> this is a >= constraint
         rhs = lb;
         range = 0.0;
         sense = 'G';
      }
      else {
         // Lower and upper bound are both infinite.
         // This is used for example in .mps files to specify alternate
         // objective functions.
         // Note that the case lb==ub was already handled above, so we just
         // pick the bound with larger magnitude and create a constraint for it.
         // Note that we replace the infinite bound by CPX_INFBOUND since
         // bounds with larger magnitude may cause other CPLEX functions to
         // fail (for example the export to LP files).
         DCHECK_GT(fabs(lb), CPX_INFBOUND);
         DCHECK_GT(fabs(ub), CPX_INFBOUND);
         if ( fabs(lb) > fabs(ub) ) {
            rhs = (lb < 0) ? -CPX_INFBOUND : CPX_INFBOUND;
            sense = 'G';
         }
         else {
            rhs = (ub < 0) ? -CPX_INFBOUND : CPX_INFBOUND;
            sense = 'L';
         }
         range = 0.0;
      }
   }

   void CplexInterface::SetConstraintBounds(int index, double lb, double ub)
   {
      InvalidateSolutionSynchronization();

      // Changing rhs, sense, or range of a constraint is not too slow.
      // Still, doing all the updates in one large operation is faster.
      // Note however that if we do not want to re-extract the full model
      // for each solve (supportIncrementalExtraction is true) then we MUST
      // update the constraint here, otherwise we lose this update information.

      if ( !supportIncrementalExtraction &&
           !(slowUpdates & SlowSetConstraintBounds) )
      {
         InvalidateModelSynchronization();
      }
      else {
        if ( constraint_is_extracted(index) ) {
            // Constraint is already extracted, so we must update its bounds
            // and its type.
            DCHECK(mLp != NULL);
            char sense;
            double range, rhs;
            MakeRhs(lb, ub, rhs, sense, range);
            CHECK_STATUS(CPXXchgrhs(mEnv, mLp, 1, &index, &lb));
            CHECK_STATUS(CPXXchgsense(mEnv, mLp, 1, &index, &sense));
            CHECK_STATUS(CPXXchgrngval(mEnv, mLp, 1, &index, &range));
         }
         else {
            // Constraint is not yet extracted. It is sufficient to mark the
            // modeling object as "out of sync"
            InvalidateModelSynchronization();
         }
      }
   }

   void CplexInterface::AddRowConstraint(MPConstraint* const ct)
   {
      // This is currently only invoked when a new constraint is created,
      // see MPSolver::MakeRowConstraint().
      // At this point we only have the lower and upper bounds of the
      // constraint. We could immediately call CPXXaddrows() here but it is
      // usually much faster to handle the fully populated constraint in
      // ExtractNewConstraints() right before the solve.
      InvalidateModelSynchronization();
   }

   void CplexInterface::AddVariable(MPVariable* const ct)
   {
      // This is currently only invoked when a new variable is created,
      // see MPSolver::MakeVar().
      // At this point the variable does not appear in any constraints or
      // the objective function. We could invoke CPXXaddcols() to immediately
      // create the variable here but it is usually much faster to handle the
      // fully setup variable in ExtractNewVariables() right before the solve.
      InvalidateModelSynchronization();
   }

   void CplexInterface::SetCoefficient(MPConstraint* const constraint,
                                       MPVariable const* const variable,
                                       double new_value, double)
   {
      InvalidateSolutionSynchronization();

      // Changing a single coefficient in the matrix is potentially pretty
      // slow since that coefficient has to be found in the sparse matrix
      // representation. So by default we don't perform this update immediately
      // but instead mark the low-level modeling object "out of sync".
      // If we want to support incremental extraction then we MUST perform
      // the modification immediately or we will lose it.

      if ( !supportIncrementalExtraction &&
           !(slowUpdates & SlowSetCoefficient) )
      {
         InvalidateModelSynchronization();
      }
      else {
         int const row = constraint->index();
         int const col = variable->index();
         if ( constraint_is_extracted(row) && variable_is_extracted(col) ) {
            // If row and column are both extracted then we can directly
            // update the modeling object
            DCHECK_LE(row, last_constraint_index_);
            DCHECK_LE(col, last_variable_index_);
            CHECK_STATUS(CPXXchgcoef(mEnv, mLp, row, col, new_value));
         }
         else {
            // If either row or column is not yet extracted then we can
            // defer the update to ExtractModel()
            InvalidateModelSynchronization();
         }
      }
   }

   void CplexInterface::ClearConstraint(MPConstraint* const constraint)
   {
      CPXDIM const row = constraint->index();
      if ( !constraint_is_extracted(row) )
         // There is nothing to do if the constraint was not even extracted.
         return;

      // Clearing a constraint means setting all coefficients in the corresponding
      // row to 0 (we cannot just delete the row since that would renumber all
      // the constraints/rows after it).
      // Modifying coefficients in the matrix is potentially pretty expensive
      // since they must be found in the sparse matrix representation. That is
      // why by default we do not modify the coefficients here but only mark
      // the low-level modeling object "out of sync".

      if ( !(slowUpdates & SlowClearConstraint) ) {
         InvalidateModelSynchronization();
      }
      else {
         InvalidateSolutionSynchronization();

         CPXDIM const len = constraint->coefficients_.size();
         unique_ptr<CPXDIM[]> rowind(new CPXDIM[len]);
         unique_ptr<CPXDIM[]> colind(new CPXDIM[len]);
         unique_ptr<double[]> val(new double[len]);
         CPXDIM j = 0;
         CoeffMap const &coeffs = constraint->coefficients_;
         for (CoeffMap::const_iterator it(coeffs.begin());
              it != coeffs.end(); ++it)
         {
            CPXDIM const col = it->first->index();
            if ( variable_is_extracted(col) ) {
               rowind[j] = row;
               colind[j] = col;
               val[j] = 0.0;
               ++j;
            }
         }
         if ( j )
            CHECK_STATUS(CPXXchgcoeflist(mEnv, mLp, j, rowind.get(),
                                         colind.get(), val.get()));
      }
   }

   void CplexInterface::SetObjectiveCoefficient(MPVariable const* const variable,
                                                double coefficient)
   {
      CPXDIM const col = variable->index();
      if ( !variable_is_extracted(col) )
         // Nothing to do if variable was not even extracted
         return;

      InvalidateSolutionSynchronization();

      // The objective function is stored as a dense vector, so updating a
      // single coefficient is O(1). So by default we update the low-level
      // modeling object here.
      // If we support incremental extraction then we have no choice but to
      // perform the update immediately.

      if ( supportIncrementalExtraction ||
           (slowUpdates & SlowSetObjectiveCoefficient) )
      {
         CHECK_STATUS(CPXXchgobj(mEnv, mLp, 1, &col, &coefficient));
      }
      else
         InvalidateModelSynchronization();
   }

   void CplexInterface::SetObjectiveOffset(double value)
   {
      // Changing the objective offset is O(1), so we always do it immediately.
      InvalidateSolutionSynchronization();
      CHECK_STATUS(CPXEsetobjoffset(mEnv, mLp, value));
   }

   void CplexInterface::ClearObjective()
   {
      InvalidateSolutionSynchronization();

      // Since the objective function is stored as a dense vector updating
      // it is O(n), so we usually perform the update immediately.
      // If we want to support incremental extraction then we have no choice
      // but to perform the update immediately.

      if ( supportIncrementalExtraction ||
           (slowUpdates & SlowClearObjective) )
      {
         CPXDIM const cols = CPXXgetnumcols(mEnv, mLp);
         unique_ptr<CPXDIM[]> ind(new CPXDIM[cols]);
         unique_ptr<double[]> zero(new double[cols]);
         CPXDIM j = 0;
         CoeffMap const &coeffs = solver_->objective_->coefficients_;
         for (CoeffMap::const_iterator it(coeffs.begin());
              it != coeffs.end(); ++it)
         {
            CPXDIM const idx = it->first->index();
            // We only need to reset variables that have been extracted.
            if ( variable_is_extracted(idx) ) {
               DCHECK_LT(idx, cols);
               ind[j] = idx;
               zero[j] = 0.0;
               ++j;
            }
         }
         if ( j > 0 )
            CHECK_STATUS(CPXXchgobj(mEnv, mLp, j, ind.get(), zero.get()));
         CHECK_STATUS(CPXEsetobjoffset(mEnv, mLp, 0.0));
      }
      else
         InvalidateModelSynchronization();
   }

   // ------ Query statistics on the solution and the solve ------

   int64 CplexInterface::iterations() const
   {
      int iter;
      if (!CheckSolutionIsSynchronized())
         return kUnknownNumberOfIterations;
      if (mMip)
         return static_cast<int64>(CPXXgetmipitcnt(mEnv, mLp));
      else
         return static_cast<int64>(CPXXgetitcnt(mEnv, mLp));
   }

   int64 CplexInterface::nodes() const
   {
      if ( mMip ) {
         if (!CheckSolutionIsSynchronized())
            return kUnknownNumberOfNodes;
         return static_cast<int64>(CPXXgetnodecnt(mEnv, mLp));
      }
      else {
         LOG(DFATAL) << "Number of nodes only available for discrete problems";
         return kUnknownNumberOfNodes;
      }
   }

   // Returns the best objective bound. Only available for discrete problems.
   double CplexInterface::best_objective_bound() const
   {
      if ( mMip ) {
         if ( !CheckSolutionIsSynchronized() || !CheckBestObjectiveBoundExists())
            // trivial_worst_objective_bound() returns sense*infinity,
            // that is meaningful even for infeasible problems
            return trivial_worst_objective_bound();
         if (solver_->variables_.size() == 0 && solver_->constraints_.size() == 0) {
            // For an empty model the best objective bound is just the offset.
            return solver_->Objective().offset();
         }
         else {
            double value = CPX_NAN;
            CHECK_STATUS(CPXXgetbestobjval(mEnv, mLp, &value));
            return value;
         }
      }
      else {
         LOG(DFATAL) << "Best objective bound only available for discrete problems";
         return trivial_worst_objective_bound();
      }
   }

   // Transform a CPLEX basis status to an MPSolver basis status.
   MPSolver::BasisStatus
   CplexInterface::xformBasisStatus(int cplex_basis_status)
   {
      switch (cplex_basis_status) {
      case CPX_AT_LOWER:
         return MPSolver::AT_LOWER_BOUND;
      case CPX_BASIC:
         return MPSolver::BASIC;
      case CPX_AT_UPPER:
         return MPSolver::AT_UPPER_BOUND;
      case CPX_FREE_SUPER:
         return MPSolver::FREE;
      default:
         LOG(DFATAL) << "Unknown CPLEX basis status";
         return MPSolver::FREE;
      }
   }

   // Returns the basis status of a row.
   MPSolver::BasisStatus CplexInterface::row_status(int constraint_index) const
   {
      if ( mMip ) {
         LOG(FATAL) << "Basis status only available for continuous problems";
         return MPSolver::FREE;
      }

      if ( CheckSolutionIsSynchronized() ) {
         if ( !mRstat ) {
            CPXDIM const rows = CPXXgetnumrows(mEnv, mLp);
            unique_ptr<int[]> data(new int[rows]);
            mRstat.swap(data);
            CHECK_STATUS(CPXXgetbase(mEnv, mLp, 0, mRstat.get()));
         }
      }
      else
         mRstat = 0;

      if ( mRstat )
         return xformBasisStatus(mRstat[constraint_index]);
      else {
         LOG(FATAL) << "Row basis status not available";
         return MPSolver::FREE;
      }
   }

   // Returns the basis status of a column.
   MPSolver::BasisStatus CplexInterface::column_status(int variable_index) const
   {
      if ( mMip ) {
         LOG(FATAL) << "Basis status only available for continuous problems";
         return MPSolver::FREE;
      }

      if ( CheckSolutionIsSynchronized() ) {
         if ( !mCstat ) {
            CPXDIM const cols = CPXXgetnumcols(mEnv, mLp);
            unique_ptr<int[]> data(new int[cols]);
            mCstat.swap(data);
            CHECK_STATUS(CPXXgetbase(mEnv, mLp, mCstat.get(), 0));
         }
      }
      else
         mCstat = 0;

      if ( mCstat )
         return xformBasisStatus(mCstat[variable_index]);
      else {
         LOG(FATAL) << "Column basis status not available";
         return MPSolver::FREE;
      }
   }

   // Extract all variables that have not yet been extracted.
   void CplexInterface::ExtractNewVariables()
   {
      // NOTE: The code assumes that a linear expression can never contain
      //       non-zero duplicates.

      InvalidateSolutionSynchronization();

      if ( !supportIncrementalExtraction ) {
         // Without incremental extraction ExtractModel() is always called
         // to extract the full model.
         CHECK(last_variable_index_ == 0 ||
               last_variable_index_ == solver_->variables_.size());
         CHECK(last_constraint_index_ == 0 ||
               last_constraint_index_ == solver_->constraints_.size());
      }

      int const last_extracted = last_variable_index_;
      int const var_count = solver_->variables_.size();
      CPXDIM newcols = var_count - last_extracted;
      if ( newcols > 0 ) {
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
            ctype[j] = var->integer() ? CPX_INTEGER : CPX_CONTINUOUS;
            colname[j] = var->name().empty() ? 0 : var->name().c_str();
            have_names = have_names || var->name().empty();
            obj[j] = solver_->objective_->GetCoefficient(var);
         }

         // Arrays for modifying the problem are setup. Update the index
         // of variables that will get extracted now. Updating indices
         // _before_ the actual extraction makes things much simpler in
         // case we support incremental extraction.
         // In case of error we just reset the indeces.
         std::vector<MPVariable*> const &variables = solver_->variables();
         for (int j = last_extracted; j < var_count; ++j) {
            CHECK(!variable_is_extracted(variables[j]->index()));
            set_variable_as_extracted(variables[j]->index(), true);
         }

         try {
            bool use_newcols = true;

            if ( supportIncrementalExtraction ) {
               // If we support incremental extraction then we must
               // update existing constraints with the new variables.
               // To do that we use CPXXaddcols() to actually create the
               // variables. This is supposed to be faster than combining
               // CPXXnewcols() and CPXXchgcoeflist().

               // For each column count the size of the intersection with
               // existing constraints.
               unique_ptr<CPXDIM[]> collen(new CPXDIM[newcols]);
               for (CPXDIM j = 0; j < newcols; ++j)
                  collen[j] = 0;
               CPXNNZ nonzeros = 0;
               // TODO: Use a bitarray to flag the constraints that actually
               //       intersect new variables?
               for (int i = 0; i < last_constraint_index_; ++i) {
                  MPConstraint const *const ct = solver_->constraints_[i];
                  CHECK(constraint_is_extracted(ct->index()));
                  CoeffMap const &coeffs = ct->coefficients_;
                  for (CoeffMap::const_iterator it(coeffs.begin());
                       it != coeffs.end(); ++it)
                  {
                     int const idx = it->first->index();
                     if ( variable_is_extracted(idx) &&
                          idx > last_variable_index_ ) {
                        collen[idx - last_variable_index_]++;
                        ++nonzeros;
                     }
                  }
               }

               if ( nonzeros > 0 ) {
                  // At least one of the new variables did intersect with an
                  // old constraint. We have to create the new columns via
                  // CPXXaddcols().
                  use_newcols = false;
                  unique_ptr<CPXNNZ[]> begin(new CPXNNZ[newcols + 2]);
                  unique_ptr<CPXDIM[]> cmatind(new CPXDIM[nonzeros]);
                  unique_ptr<double[]> cmatval(new double[nonzeros]);

                  // Here is how cmatbeg[] is setup:
                  // - it is initialized as
                  //     [ 0, 0, collen[0], collen[0]+collen[1], ... ]
                  //   so that cmatbeg[j+1] tells us where in cmatind[] and
                  //   cmatval[] we need to put the next nonzero for column
                  //   j
                  // - after nonzeros have been setup the array looks like
                  //     [ 0, collen[0], collen[0]+collen[1], ... ]
                  //   so that it is the correct input argument for CPXXaddcols
                  CPXNNZ *cmatbeg = begin.get();
                  cmatbeg[0] = 0;
                  cmatbeg[1] = 0;
                  ++cmatbeg;
                  for (CPXDIM j = 0; j < newcols; ++j)
                     cmatbeg[j + 1] = cmatbeg[j] + collen[j];

                  for (int i = 0; i < last_constraint_index_; ++i) {
                     MPConstraint const *const ct = solver_->constraints_[i];
                     CPXDIM const row = ct->index();
                     CoeffMap const &coeffs = ct->coefficients_;
                     for (CoeffMap::const_iterator it(coeffs.begin());
                          it != coeffs.end(); ++it)
                     {
                        int const idx = it->first->index();
                        if ( variable_is_extracted(idx) &&
                             idx > last_variable_index_ ) {
                           CPXNNZ const nz = cmatbeg[idx]++;
                           cmatind[nz] = idx;
                           cmatval[nz] = it->second;
                        }
                     }
                  }
                  --cmatbeg;
                  CHECK_STATUS(CPXXaddcols(mEnv, mLp, newcols, nonzeros,
                                           obj.get(), cmatbeg,
                                           cmatind.get(), cmatval.get(),
                                           lb.get(), ub.get(),
                                           have_names ? colname.get() : 0));
               }
            }
            if ( use_newcols ) {
               // Either incremental extraction is not supported or none of
               // the new variables did intersect an existing constraint.
               // We can just use CPXXnewcols() to create the new variables.
               CHECK_STATUS(CPXXnewcols(mEnv, mLp, newcols, obj.get(),
                                        lb.get(), ub.get(),
                                        mMip ? ctype.get() : 0,
                                        have_names ? colname.get() : 0));
            }
            else {
               // Incremental extraction: we must update the ctype of the
               // newly created variables (CPXXaddcols() does not allow
               // specifying the ctype)
               if ( mMip ) {
                  // Query the actual number of columns in case we did not
                  // manage to extract all columns.
                  int const cols = CPXXgetnumcols(mEnv, mLp);
                  unique_ptr<CPXDIM[]> ind(new CPXDIM[newcols]);
                  for (int j = last_extracted; j < cols; ++j)
                     ind[j - last_extracted] = j;
                  CHECK_STATUS(CPXXchgctype(mEnv, mLp, cols - last_extracted,
                                            ind.get(), ctype.get()));
               }
            }
         }
         catch (...) {
            // Undo all changes in case of error.
            CPXDIM const cols = CPXXgetnumcols(mEnv, mLp);
            if ( cols > last_extracted )
               (void)CPXXdelcols(mEnv, mLp, last_extracted, cols - 1);
            std::vector<MPVariable*> const &variables = solver_->variables();
            int const size = variables.size();
            for (int j = last_extracted; j < size; ++j)
              set_variable_as_extracted(j, false);
            throw;
         }
      }
   }

   // Extract constraints that have not yet been extracted.
   void CplexInterface::ExtractNewConstraints()
   {
      // NOTE: The code assumes that a linear expression can never contain
      //       non-zero duplicates.

      if ( !supportIncrementalExtraction ) {
         // Without incremental extraction ExtractModel() is always called
         // to extract the full model.
         CHECK(last_variable_index_ == 0 ||
               last_variable_index_ == solver_->variables_.size());
         CHECK(last_constraint_index_ == 0 ||
               last_constraint_index_ == solver_->constraints_.size());
      }

      CPXDIM const offset = last_constraint_index_;
      CPXDIM const total = solver_->constraints_.size();

      if ( total > offset ) {
         // There are constraints that are not yet extracted.

         InvalidateSolutionSynchronization();

         CPXDIM newCons = total - offset;
         CPXDIM const cols = CPXXgetnumcols(mEnv, mLp);
         DCHECK_EQ(last_variable_index_, cols);
         CPXDIM const chunk = 10; // max number of rows to add in one shot

         // Update indices of new constraints _before_ actually extracting
         // them. In case of error we will just reset the indices.
         for (CPXDIM c = offset; c < total; ++c)
            set_constraint_as_extracted(c, true);

         try {
            unique_ptr<CPXDIM[]> rmatind(new CPXDIM[cols]);
            unique_ptr<double[]> rmatval(new double[cols]);
            unique_ptr<CPXNNZ[]> rmatbeg(new CPXNNZ[chunk]);
            unique_ptr<char[]> sense(new char[chunk]);
            unique_ptr<double[]> rhs(new double[chunk]);
            unique_ptr<char const *[]> name(new char const*[chunk]);
            unique_ptr<double[]> rngval(new double[chunk]);
            unique_ptr<CPXDIM[]> rngind(new CPXDIM[chunk]);
            bool haveRanges = false;

            // Loop over the new constraints, collecting rows for up to
            // CHUNK constraints into the arrays so that adding constraints
            // is faster.
            for (CPXDIM c = 0; c < newCons; /* nothing */) {
               // Collect up to CHUNK constraints into the arrays.
               CPXDIM nextRow = 0;
               CPXNNZ nextNz = 0;
               for (/* nothing */; c < newCons && nextRow < chunk; ++c, ++nextRow) {
                  MPConstraint const *const ct = solver_->constraints_[offset + c];

                  // Stop if there is not enough room in the arrays
                  // to add the current constraint.
                  if ( nextNz + ct->coefficients_.size() > cols ) {
                     DCHECK_GT(nextRow, 0);
                     break;
                  }

                  // Setup right-hand side of constraint.
                  MakeRhs(ct->lb(), ct->ub(), rhs[nextRow], sense[nextRow], rngval[nextRow]);
                  haveRanges = haveRanges || (rngval[nextRow] != 0.0);
                  rngind[nextRow] = offset + c;

                  // Setup left-hand side of constraint.
                  rmatbeg[nextRow] = nextNz;
                  CoeffMap const &coeffs = ct->coefficients_;
                  for (CoeffMap::const_iterator it(coeffs.begin());
                       it != coeffs.end(); ++it)
                  {
                     CPXDIM const idx = it->first->index();
                     if ( variable_is_extracted(idx) ) {
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
               if ( nextRow > 0 ) {
                  CHECK_STATUS(CPXXaddrows(mEnv, mLp, 0, nextRow, nextNz,
                                           rhs.get(), sense.get(),
                                           rmatbeg.get(), rmatind.get(),
                                           rmatval.get(), 0,
                                           name.get()));
                  if ( haveRanges ) {
                     CHECK_STATUS(CPXXchgrngval(mEnv, mLp, nextRow,
                                                rngind.get(), rngval.get()));
                  }
               }
            }
         }
         catch (...) {
            // Undo all changes in case of error.
            CPXDIM const rows = CPXXgetnumrows(mEnv, mLp);
            if ( rows > offset )
               (void)CPXXdelrows(mEnv, mLp, offset, rows - 1);
            std::vector<MPConstraint*> const &constraints = solver_->constraints();
            int const size = constraints.size();
            for (int i = offset; i < size; ++i)
              set_constraint_as_extracted(i, false);
            throw;
         }
      }
   }

   // Extract the objective function.
   void CplexInterface::ExtractObjective()
   {
      // NOTE: The code assumes that the objective expression does not contain
      //       any non-zero duplicates.

      CPXDIM const cols = CPXXgetnumcols(mEnv, mLp);
      DCHECK_EQ(last_variable_index_, cols);

      unique_ptr<CPXDIM[]> ind(new CPXDIM[cols]);
      unique_ptr<double[]> val(new double[cols]);
      for (CPXDIM j = 0; j < cols; ++j) {
         ind[j] = j;
         val[j] = 0.0;
      }

      CoeffMap const &coeffs = solver_->objective_->coefficients_;
      for (CoeffMap::const_iterator it = coeffs.begin(); it != coeffs.end(); ++it)
      {
         CPXDIM const idx = it->first->index();
         if ( variable_is_extracted(idx) ) {
            DCHECK_LT(idx, cols);
            val[idx] = it->second;
         }
      }

      CHECK_STATUS(CPXXchgobj(mEnv, mLp, cols, ind.get(), val.get()));
      CHECK_STATUS(CPXEsetobjoffset(mEnv, mLp, solver_->Objective().offset()));
   }

   // ------ Parameters  -----

   void CplexInterface::SetParameters(const MPSolverParameters& param)
   {
      SetCommonParameters(param);
      if (mMip)
         SetMIPParameters(param);
   }

   void CplexInterface::SetRelativeMipGap(double value)
   {
      if (mMip) {
         CHECK_STATUS(CPXXsetdblparam(mEnv, CPX_PARAM_EPGAP, value));
      }
      else {
         LOG(WARNING) << "The relative MIP gap is only available "
                      << "for discrete problems.";
      }
   }

   void CplexInterface::SetPrimalTolerance(double value)
   {
      CHECK_STATUS(CPXXsetdblparam(mEnv, CPX_PARAM_EPRHS, value));
   }

   void CplexInterface::SetDualTolerance(double value)
   {
      CHECK_STATUS(CPXXsetdblparam(mEnv, CPX_PARAM_EPOPT, value));
   }

   void CplexInterface::SetPresolveMode(int value)
   {
      MPSolverParameters::PresolveValues const presolve
         = static_cast<MPSolverParameters::PresolveValues>(value);

      switch (presolve) {
      case MPSolverParameters::PRESOLVE_OFF:
         CHECK_STATUS(CPXXsetintparam(mEnv, CPX_PARAM_PREIND, CPX_OFF));
         return;
      case MPSolverParameters::PRESOLVE_ON:
         CHECK_STATUS(CPXXsetintparam(mEnv, CPX_PARAM_PREIND, CPX_ON));
         return;
      }
      SetIntegerParamToUnsupportedValue(MPSolverParameters::PRESOLVE, value);
   }

   // Sets the scaling mode.
   void CplexInterface::SetScalingMode(int value)
   {
      MPSolverParameters::ScalingValues const scaling
         = static_cast<MPSolverParameters::ScalingValues>(value);

      switch (scaling) {
      case MPSolverParameters::SCALING_OFF:
         CHECK_STATUS(CPXXsetintparam(mEnv, CPX_PARAM_SCAIND, -1));
         break;
      case MPSolverParameters::SCALING_ON:
         // TODO: 0 is equilibrium scaling (the default), CPLEX also supports
         //       1 aggressive scaling
         CHECK_STATUS(CPXXsetintparam(mEnv, CPX_PARAM_SCAIND, 0));
         break;
      }
   }

   // Sets the LP algorithm : primal, dual or barrier. Note that CPLEX offers other LP algorithm (e.g. network) and automatic selection
   void CplexInterface::SetLpAlgorithm(int value)
   {
      MPSolverParameters::LpAlgorithmValues const algorithm
         = static_cast<MPSolverParameters::LpAlgorithmValues>(value);

      int alg = CPX_ALG_NONE;

      switch (algorithm) {
      case MPSolverParameters::DUAL:    alg = CPX_ALG_DUAL;    break;
      case MPSolverParameters::PRIMAL:  alg = CPX_ALG_PRIMAL;  break;
      case MPSolverParameters::BARRIER: alg = CPX_ALG_BARRIER; break;
      }

      if ( alg == CPX_ALG_NONE )
         SetIntegerParamToUnsupportedValue(MPSolverParameters::LP_ALGORITHM, value);
      else {
         CHECK_STATUS(CPXXsetintparam(mEnv, CPX_PARAM_LPMETHOD, alg));
         if ( mMip ) {
            // For MIP we have to change two more parameters to specify the
            // algorithm that is used to solve LP relaxations.
            CHECK_STATUS(CPXXsetintparam(mEnv, CPX_PARAM_STARTALG, alg));
            CHECK_STATUS(CPXXsetintparam(mEnv, CPX_PARAM_SUBALG, alg));
         }
      }
   }

   bool CplexInterface::ReadParameterFile(std::string const &filename)
   {
      // Return true on success and false on error.
      return CPXXreadcopyparam(mEnv, filename.c_str()) == 0;
   }

   std::string CplexInterface::ValidFileExtensionForParameterFile() const
   {
      return ".prm";
   }

   MPSolver::ResultStatus CplexInterface::Solve(MPSolverParameters const &param)
   {
      int status;

      // Delete chached information
      mCstat = 0;
      mRstat = 0;

      WallTimer timer;
      timer.Start();

      // Set incrementality
      MPSolverParameters::IncrementalityValues const inc
         = static_cast<MPSolverParameters::IncrementalityValues>(param.GetIntegerParam(MPSolverParameters::INCREMENTALITY));
      switch (inc) {
      case MPSolverParameters::INCREMENTALITY_OFF:
         Reset(); /* This should not be required but re-extracting everything
                   * may be faster, so we do it. */
         CHECK_STATUS(CPXXsetintparam(mEnv, CPX_PARAM_ADVIND, 0));
         break;
      case MPSolverParameters::INCREMENTALITY_ON:
         CHECK_STATUS(CPXXsetintparam(mEnv, CPX_PARAM_ADVIND, 2));
         break;
      }

      // Extract the model to be solved.
      // If we don't support incremental extraction and the low-level modeling
      // is out of sync then we have to re-extract everything. Note that this
      // will lose MIP starts or advanced basis information from a previous
      // solve.
      if ( !supportIncrementalExtraction &&
           sync_status_ == MUST_RELOAD )
         Reset();
      ExtractModel();
      VLOG(1) << StringPrintf("Model build in %.3f seconds.", timer.Get());

      // Set log level.
      CHECK_STATUS(CPXXsetintparam(mEnv, CPX_PARAM_SCRIND, quiet() ? CPX_OFF : CPX_ON));

      // Set parameters.
      // NOTE: We must invoke SetSolverSpecificParametersAsString() _first_.
      //       Its current implementation invokes ReadParameterFile() which in
      //       turn invokes CPXXreadcopyparam(). The latter will _overwrite_
      //       all current parameter settings in the environment.
      solver_->SetSolverSpecificParametersAsString(solver_->solver_specific_parameter_string_);
      SetParameters(param);
      if (solver_->time_limit()) {
         VLOG(1) << "Setting time limit = " << solver_->time_limit() << " ms.";
         CHECK_STATUS(CPXXsetdblparam(mEnv, CPX_PARAM_TILIM, solver_->time_limit() *1e-3));
      }

      // Solve.
      // Do not CHECK_STATUS here since some errors (for example CPXERR_NO_MEMORY)
      // still allow us to query useful information.
      timer.Restart();
      if (mMip) {
         status = CPXXmipopt(mEnv, mLp);
      }
      else {
         status = CPXXlpopt(mEnv, mLp);
      }

      // Disable screen output right after solve
      (void)CPXXsetintparam(mEnv, CPX_PARAM_SCRIND, CPX_OFF);

      if ( status ) {
         VLOG(1) << StringPrintf("Failed to optimize MIP. Error %d", status);
         // NOTE: We do not return immediately since there may be information
         //       to grab (for example an incumbent)
      }
      else {
         VLOG(1) << StringPrintf("Solved in %.3f seconds.", timer.Get());
      }

      int const cpxstat = CPXXgetstat(mEnv, mLp);
      VLOG(1) << StringPrintf("CPLEX solution status %d.", cpxstat);

      // Figure out what solution we have.
      int solnmethod, solntype, pfeas, dfeas;
      CHECK_STATUS(CPXXsolninfo(mEnv, mLp, &solnmethod, &solntype, &pfeas, &dfeas));
      bool const feasible = pfeas != 0;

      // Get problem dimensions for solution queries below.
      CPXDIM const rows = CPXXgetnumrows(mEnv, mLp);
      CPXDIM const cols = CPXXgetnumcols(mEnv, mLp);
      DCHECK_EQ(rows, solver_->constraints_.size());
      DCHECK_EQ(cols, solver_->variables_.size());

      // Capture objective function value.
      objective_value_ = CPX_NAN;
      if ( pfeas )
         CHECK_STATUS(CPXXgetobjval(mEnv, mLp, &objective_value_));
      VLOG(1) << "objective = " << objective_value_;

      // Capture primal and dual solutions
      if ( mMip ) {
         // If there is a primal feasible solution then capture it.
         if ( pfeas ) {
            if ( cols > 0 ) {
               unique_ptr<double[]> x(new double[cols]);
               CHECK_STATUS(CPXXgetx(mEnv, mLp, x.get(), 0, cols - 1));
               for (int i = 0; i < solver_->variables_.size(); ++i) {
                  MPVariable* const var = solver_->variables_[i];
                  var->set_solution_value(x[i]);
                  VLOG(3) << var->name() << ": value =" << x[i];
               }
            }
         }
         else {
            for (int i = 0; i < solver_->variables_.size(); ++i)
               solver_->variables_[i]->set_solution_value(CPX_NAN);
         }

         // MIP does not have duals
         for (int i = 0; i < solver_->variables_.size(); ++i)
            solver_->variables_[i]->set_reduced_cost(CPX_NAN);
         for (int i = 0; i < solver_->constraints_.size(); ++i)
            solver_->constraints_[i]->set_dual_value(CPX_NAN);
      }
      else {
         // Continuous problem.
         if ( cols > 0 ) {
            unique_ptr<double[]> x(new double[cols]);
            unique_ptr<double[]> dj(new double[cols]);
            if ( pfeas )
               CHECK_STATUS(CPXXgetx(mEnv, mLp, x.get(), 0, cols - 1));
            if ( dfeas )
               CHECK_STATUS(CPXXgetdj(mEnv, mLp, dj.get(), 0, cols - 1));
            for (int i = 0; i < solver_->variables_.size(); ++i) {
               MPVariable* const var = solver_->variables_[i];
               var->set_solution_value(x[i]);
               bool value = false, dual = false;

               if ( pfeas ) {
                  var->set_solution_value(x[i]);
                  value = true;
               }
               else
                  var->set_solution_value(CPX_NAN);
               if ( dfeas ) {
                  var->set_reduced_cost(dj[i]);
                  dual = true;
               }
               else
                  var->set_reduced_cost(CPX_NAN);
               VLOG(3) << var->name() << ":"
                       << (value ? StringPrintf("  value = %f", x[i]) : "")
                       << (dual ? StringPrintf("  reduced cost = %f", dj[i]) : "");
            }
         }

         if ( rows > 0 ) {
            unique_ptr<double[]> pi(new double[rows]);
            if ( dfeas )
               CHECK_STATUS(CPXXgetpi(mEnv, mLp, pi.get(), 0, rows - 1));
            for (int i = 0; i < solver_->constraints_.size(); ++i) {
               MPConstraint* const ct = solver_->constraints_[i];
               bool dual = false;
               if ( dfeas ) {
                  ct->set_dual_value(pi[i]);
                  dual = true;
               }
               else
                  ct->set_dual_value(CPX_NAN);
               VLOG(4) << "row " << ct->index() << ":"
                       << (dual  ? StringPrintf("  dual = %f", pi[i]) : "");
            }
         }
      }

      // Map CPLEX status to more generic solution status in MPSolver
      switch (cpxstat) {
      case CPX_STAT_OPTIMAL:
      case CPXMIP_OPTIMAL:
         result_status_ = MPSolver::OPTIMAL;
         break;
      case CPXMIP_OPTIMAL_TOL:
         // To be consistent with the other solvers.
         result_status_ = MPSolver::OPTIMAL;
         break;
      case CPX_STAT_INFEASIBLE:
      case CPXMIP_INFEASIBLE:
         result_status_ = MPSolver::INFEASIBLE;
         break;
      case CPX_STAT_UNBOUNDED:
      case CPXMIP_UNBOUNDED:
         result_status_ = MPSolver::UNBOUNDED;
         break;
      case CPX_STAT_INForUNBD:
      case CPXMIP_INForUNBD:
         result_status_ = MPSolver::INFEASIBLE;
         break;
      default:
         result_status_ = feasible ? MPSolver::FEASIBLE : MPSolver::ABNORMAL;
         break;
      }

      sync_status_ = SOLUTION_SYNCHRONIZED;
      return result_status_;
   }

   MPSolverInterface *BuildCplexInterface(bool mip, MPSolver *const solver)
   {
      return new CplexInterface(solver, mip);
   }

}  // namespace operations_research
#endif  // #if defined(USE_CPLEX)
