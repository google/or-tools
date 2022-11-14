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

#include <limits>
#include <memory>
#include <exception>

#include "absl/strings/str_format.h"
#include "ortools/base/integral_types.h"
#include "ortools/base/logging.h"
#include "ortools/base/timer.h"
#include "ortools/linear_solver/linear_solver.h"

#if defined(USE_SIRIUS)

extern "C" {
#include "srs_api.h"
}

#define SRS_INTEGER 2
#define SRS_CONTINUOUS 1

//# define EN_BASE                 0  
//# define EN_BASE_LIBRE           1  
//# define EN_BASE_SUR_BORNE_INF   2
//# define EN_BASE_SUR_BORNE_SUP   3
//# define HORS_BASE_SUR_BORNE_INF 4
//# define HORS_BASE_SUR_BORNE_SUP 5 
//# define HORS_BASE_A_ZERO        6  /* Pour les variables non bornees qui restent hors base */

//FREE = 0,
//AT_LOWER_BOUND,
//AT_UPPER_BOUND,
//FIXED_VALUE,
//BASIC

enum SRS_BASIS_STATUS {
	SRS_BASIC = EN_BASE,
	SRS_BASIC_FREE = EN_BASE_LIBRE,
	SRS_AT_LOWER = EN_BASE_SUR_BORNE_INF,
	SRS_AT_UPPER = EN_BASE_SUR_BORNE_SUP,
	SRS_FREE_LOWER = HORS_BASE_SUR_BORNE_INF,
	SRS_FREE_UPPER = HORS_BASE_SUR_BORNE_SUP,
	SRS_FREE_ZERO = HORS_BASE_A_ZERO,
};

// In case we need to return a double but don't have a value for that
// we just return a NaN.
#if !defined(CPX_NAN)
#define SRS_NAN std::numeric_limits<double>::quiet_NaN()
#endif

// The argument to this macro is the invocation of a SRS function that
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
	// 1:1 corresponence between MPVariable instances and SIRIUS columns: the
	// index of an extracted variable is the column index in the SIRIUS model.
	// Similiar for instances of MPConstraint: the index of the constraint in
	// the model is the row index in the SIRIUS model.
	class SiriusInterface : public MPSolverInterface {
	public:
		// NOTE: 'mip' specifies the type of the problem (either continuous or
		//       mixed integer. This type is fixed for the lifetime of the
		//       instance. There are no dynamic changes to the model type.
		explicit SiriusInterface(MPSolver *const solver, bool mip);
		~SiriusInterface();

		// Sets the optimization direction (min/max).
		virtual void SetOptimizationDirection(bool maximize);

		// ----- Solve -----
		// Solve the problem using the parameter values specified.
		virtual MPSolver::ResultStatus Solve(MPSolverParameters const &param);

		// Writes the model.
		void Write(const std::string& filename) override;

		// ----- Model modifications and extraction -----
		// Resets extracted model
		virtual void Reset();

		virtual void SetVariableBounds(int var_index, double lb, double ub);
		virtual void SetVariableInteger(int var_index, bool integer);
		virtual void SetConstraintBounds(int row_index, double lb, double ub);

		virtual void AddRowConstraint(MPConstraint *const ct);
		virtual void AddVariable(MPVariable *const var);
		virtual void SetCoefficient(MPConstraint *const constraint,
			MPVariable const *const variable,
			double new_value, double old_value);

		// Clear a constraint from all its terms.
		virtual void ClearConstraint(MPConstraint *const constraint);
		// Change a coefficient in the linear objective
		virtual void SetObjectiveCoefficient(MPVariable const *const variable,
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

		bool SetSolverSpecificParametersAsString(const std::string& parameters) override;

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

		virtual void *underlying_solver() { return reinterpret_cast<void *>(mLp); }

		virtual double ComputeExactConditionNumber() const {
			if (!IsContinuous()) {
				LOG(DFATAL) << "ComputeExactConditionNumber not implemented for"
					<< " SIRIUS_MIXED_INTEGER_PROGRAMMING";
				return 0.0;
			}

			// TODO(user,user): Not yet working.
			LOG(DFATAL) << "ComputeExactConditionNumber not implemented for"
				<< " SIRIUS_LINEAR_PROGRAMMING";
			return 0.0;
		}

	protected:
		// Set all parameters in the underlying solver.
		virtual void SetParameters(MPSolverParameters const &param);
		// Set each parameter in the underlying solver.
		virtual void SetRelativeMipGap(double value);
		virtual void SetPrimalTolerance(double value);
		virtual void SetDualTolerance(double value);
		virtual void SetPresolveMode(int value) override;
		virtual void SetScalingMode(int value);
		virtual void SetLpAlgorithm(int value);

		virtual bool ReadParameterFile(std::string const &filename);
		virtual absl::Status SetNumThreads(int num_threads) override;
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

		// Transform SIRIUS basis status to MPSolver basis status.
		static MPSolver::BasisStatus xformBasisStatus(char sirius_basis_status);

	private:
		SRS_PROBLEM * mLp;
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
		// SIRIUS has no method to query the basis status of a single variable.
		// Hence we query the status only once and cache the array. This is
		// much faster in case the basis status of more than one row/column
		// is required.
		unique_ptr<char[]> mutable mCstat;
		unique_ptr<char[]> mutable mRstat;

		// Setup the right-hand side of a constraint from its lower and upper bound.
		static void MakeRhs(double lb, double ub, double &rhs, char &sense,
			double &range);

		std::map<int , std::vector<std::pair<int, double> > > fixedOrderCoefficientsPerConstraint;

		// vector to store TypeDeBorneDeLaVariable values
		std::vector<int> varBoundsTypeValues;
	};

	// Creates a LP/MIP instance.
	SiriusInterface::SiriusInterface(MPSolver *const solver, bool mip)
		: MPSolverInterface(solver),
		mLp(NULL),
		mMip(mip),
		slowUpdates(static_cast<SlowUpdates>(SlowSetObjectiveCoefficient |
			SlowClearObjective)),
		supportIncrementalExtraction(false),
		mCstat(),
		mRstat() {
		//google::InitGoogleLogging("Sirius");
		int status;

		char const *name = solver_->name_.c_str();
		mLp = SRScreateprob();
		DCHECK(mLp != nullptr);  // should not be NULL if status=0
		//FIXME CHECK_STATUS(SRSloadlp(mLp, "newProb", 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0));

		//FIXME CHECK_STATUS(SRSchgobjsense(mLp, maximize_ ? SRS_OBJ_MAXIMIZE : SRS_OBJ_MINIMIZE));
	}

	SiriusInterface::~SiriusInterface() {
		CHECK_STATUS(SRSfreeprob(mLp));
		//google::ShutdownGoogleLogging();
	}

	std::string SiriusInterface::SolverVersion() const {
		// We prefer SRSversionnumber() over SRSversion() since the
		// former will never pose any encoding issues.
		return absl::StrFormat("SIRIUS library version %s", SRSversion());
	}

	// ------ Model modifications and extraction -----

	void SiriusInterface::Reset() {
		// Instead of explicitly clearing all modeling objects we
		// just delete the problem object and allocate a new one.
		CHECK_STATUS(SRSfreeprob(mLp));

		const char *const name = solver_->name_.c_str();
		mLp = SRScreateprob();
		DCHECK(mLp != nullptr);  // should not be NULL if status=0
		//FIXME CHECK_STATUS(SRSloadlp(mLp, "newProb", 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0));

		//FIXME CHECK_STATUS(SRSchgobjsense(mLp, maximize_ ? SRS_OBJ_MAXIMIZE : SRS_OBJ_MINIMIZE));

		ResetExtractionInformation();
		mCstat = 0;
		mRstat = 0;
	}

	void SiriusInterface::SetOptimizationDirection(bool maximize) {
		InvalidateSolutionSynchronization();
		//FIXME SRSchgobjsense(mLp, maximize ? SRS_OBJ_MAXIMIZE : SRS_OBJ_MINIMIZE);
	}

	void SiriusInterface::SetVariableBounds(int var_index, double lb, double ub) {
		InvalidateSolutionSynchronization();

		// Changing the bounds of a variable is fast. However, doing this for
		// many variables may still be slow. So we don't perform the update by
		// default. However, if we support incremental extraction
		// (supportIncrementalExtraction is true) then we MUST perform the
		// update here or we will lose it.

		//if (!supportIncrementalExtraction && !(slowUpdates & SlowSetVariableBounds)) {
		//	InvalidateModelSynchronization();
		//}
		//else
		{
			if (variable_is_extracted(var_index)) {
				// Variable has already been extracted, so we must modify the
				// modeling object.
				DCHECK_LT(var_index, last_variable_index_);
				int const idx[1] = { var_index };
				double lb_l = (lb == -MPSolver::infinity() ? -SRS_infinite : lb);
				double ub_l = (ub == MPSolver::infinity() ? SRS_infinite : ub);
				CHECK_STATUS(SRSchgbds(mLp, 1, idx, &lb_l, &ub_l));
			}
			else {
				// Variable is not yet extracted. It is sufficient to just mark
				// the modeling object "out of sync"
				InvalidateModelSynchronization();
			}
		}
	}

	// Modifies integrality of an extracted variable.
	void SiriusInterface::SetVariableInteger(int var_index, bool integer) {
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
			!(slowUpdates && SlowSetVariableInteger)) {
			InvalidateModelSynchronization();
		}
		else {
			if (mMip) {
				if (variable_is_extracted(var_index)) {
					// Variable is extracted. Change the type immediately.
					// TODO: Should we check the current type and don't do anything
					//       in case the type does not change?
					DCHECK_LE(var_index, SRSgetnbcols(mLp));
					char const type = integer ? SRS_INTEGER : SRS_CONTINUOUS;
					throw std::logic_error("Not implemented");
					//FIXME CHECK_STATUS(SRSchgcoltype(mLp, 1, &var_index, &type));
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
	void SiriusInterface::MakeRhs(double lb, double ub, double &rhs, char &sense,
		double &range) {
		if (lb == ub) {
			// Both bounds are equal -> this is an equality constraint
			rhs = lb;
			range = 0.0;
			sense = '=';
		}
		else if (lb > (-SRS_infinite) && ub < SRS_infinite) {
			// Both bounds are finite -> this is a ranged constraint
			throw std::logic_error("Sirius does not handle ranged constraint.");
			if (ub < lb) {
				CHECK_STATUS(-1);
			}
			CHECK_STATUS(-1);
		}
		else if (ub < SRS_infinite ||
			(std::abs(ub) == SRS_infinite && std::abs(lb) > SRS_infinite)) {
			// Finite upper, infinite lower bound -> this is a <= constraint
			rhs = ub;
			range = 0.0;
			sense = '<';
		}
		else if (lb > (-SRS_infinite) ||
			(std::abs(lb) == SRS_infinite && std::abs(ub) > SRS_infinite)) {
			// Finite lower, infinite upper bound -> this is a >= constraint
			rhs = lb;
			range = 0.0;
			sense = '>';
		}
		else {
			// Lower and upper bound are both infinite.
			// This is used for example in .mps files to specify alternate
			// objective functions.
			// Note that the case lb==ub was already handled above, so we just
			// pick the bound with larger magnitude and create a constraint for it.
			// Note that we replace the infinite bound by SRS_infinite since
			// bounds with larger magnitude may cause other SIRIUS functions to
			// fail (for example the export to LP files).
			DCHECK_GT(std::abs(lb), SRS_infinite);
			DCHECK_GT(std::abs(ub), SRS_infinite);
			if (std::abs(lb) > std::abs(ub)) {
				rhs = (lb < 0) ? -SRS_infinite : SRS_infinite;
				sense = '>';
			}
			else {
				rhs = (ub < 0) ? -SRS_infinite : SRS_infinite;
				sense = '<';
			}
			range = 0.0;
		}
	}

	void SiriusInterface::SetConstraintBounds(int index, double lb, double ub) {
		InvalidateSolutionSynchronization();

		// Changing rhs, sense, or range of a constraint is not too slow.
		// Still, doing all the updates in one large operation is faster.
		// Note however that if we do not want to re-extract the full model
		// for each solve (supportIncrementalExtraction is true) then we MUST
		// update the constraint here, otherwise we lose this update information.

		//if (!supportIncrementalExtraction &&
		//	!(slowUpdates & SlowSetConstraintBounds)) {
		//	InvalidateModelSynchronization();
		//}
		//else
		{
			if (constraint_is_extracted(index)) {
				// Constraint is already extracted, so we must update its bounds
				// and its type.
				DCHECK(mLp != NULL);
				char sense;
				double range, rhs;
				MakeRhs(lb, ub, rhs, sense, range);
				CHECK_STATUS(SRSchgrhs(mLp, 1, &index, &rhs));
				CHECK_STATUS(SRSchgsens(mLp, 1, &index, &sense));
				CHECK_STATUS(SRSchgrangeval(mLp, 1, &index, &range));
			}
			else {
				// Constraint is not yet extracted. It is sufficient to mark the
				// modeling object as "out of sync"
				InvalidateModelSynchronization();
			}
		}
	}

	void SiriusInterface::AddRowConstraint(MPConstraint *const ct) {
		// This is currently only invoked when a new constraint is created,
		// see MPSolver::MakeRowConstraint().
		// At this point we only have the lower and upper bounds of the
		// constraint. We could immediately call SRSaddrows() here but it is
		// usually much faster to handle the fully populated constraint in
		// ExtractNewConstraints() right before the solve.
		InvalidateModelSynchronization();
	}

	void SiriusInterface::AddVariable(MPVariable *const ct) {
		// This is currently only invoked when a new variable is created,
		// see MPSolver::MakeVar().
		// At this point the variable does not appear in any constraints or
		// the objective function. We could invoke SRSaddcols() to immediately
		// create the variable here but it is usually much faster to handle the
		// fully setup variable in ExtractNewVariables() right before the solve.
		InvalidateModelSynchronization();
	}

	void SiriusInterface::SetCoefficient(MPConstraint *const constraint,
		MPVariable const *const variable,
		double new_value, double) {
		InvalidateSolutionSynchronization();

		fixedOrderCoefficientsPerConstraint[constraint->index()].push_back(std::make_pair(variable->index(), new_value));

		// Changing a single coefficient in the matrix is potentially pretty
		// slow since that coefficient has to be found in the sparse matrix
		// representation. So by default we don't perform this update immediately
		// but instead mark the low-level modeling object "out of sync".
		// If we want to support incremental extraction then we MUST perform
		// the modification immediately or we will lose it.

		if (!supportIncrementalExtraction && !(slowUpdates & SlowSetCoefficient)) {
			InvalidateModelSynchronization();
		}
		else {
			int const row = constraint->index();
			int const col = variable->index();
			if (constraint_is_extracted(row) && variable_is_extracted(col)) {
				// If row and column are both extracted then we can directly
				// update the modeling object
				DCHECK_LE(row, last_constraint_index_);
				DCHECK_LE(col, last_variable_index_);
				//FIXME CHECK_STATUS(SRSchgcoef(mLp, row, col, new_value));
			}
			else {
				// If either row or column is not yet extracted then we can
				// defer the update to ExtractModel()
				InvalidateModelSynchronization();
			}
		}
	}

	void SiriusInterface::ClearConstraint(MPConstraint *const constraint) {
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
		}
		else {
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
			if (j) {
				//FIXME CHECK_STATUS(SRSchgmcoef(mLp, j, rowind.get(), colind.get(), val.get()));
			}
		}
	}

	void SiriusInterface::SetObjectiveCoefficient(MPVariable const *const variable,
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
			CHECK_STATUS(SRSchgobj(mLp, 1, &col, &coefficient));
		}
		else
			InvalidateModelSynchronization();
	}

	void SiriusInterface::SetObjectiveOffset(double value) {
		// Changing the objective offset is O(1), so we always do it immediately.
		InvalidateSolutionSynchronization();
		throw std::logic_error("Not implemented");
		//FIXME CHECK_STATUS(SRSsetobjoffset(mLp, value));
	}

	void SiriusInterface::ClearObjective() {
		InvalidateSolutionSynchronization();

		// Since the objective function is stored as a dense vector updating
		// it is O(n), so we usually perform the update immediately.
		// If we want to support incremental extraction then we have no choice
		// but to perform the update immediately.

		if (supportIncrementalExtraction || (slowUpdates & SlowClearObjective)) {
			int const cols = SRSgetnbcols(mLp);
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
			if (j > 0) CHECK_STATUS(SRSchgobj(mLp, j, ind.get(), zero.get()));
			//FIXME CHECK_STATUS(SRSsetobjoffset(mLp, 0.0));
		}
		else
			InvalidateModelSynchronization();
	}

	// ------ Query statistics on the solution and the solve ------

	int64_t SiriusInterface::iterations() const {
		int iter = 0;
		if (!CheckSolutionIsSynchronized()) return kUnknownNumberOfIterations;
		CHECK_STATUS(SRSgetspxitercount(mLp, &iter));
		return static_cast<int64_t>(iter);
	}

	int64_t SiriusInterface::nodes() const {
		if (mMip) {
			int nodes = 0;
			if (!CheckSolutionIsSynchronized()) return kUnknownNumberOfNodes;
			CHECK_STATUS(SRSgetmipnodecount(mLp, &nodes));
			return static_cast<int64_t>(nodes);
		}
		else {
			LOG(DFATAL) << "Number of nodes only available for discrete problems";
			return kUnknownNumberOfNodes;
		}
	}

	// Transform a SIRIUS basis status to an MPSolver basis status.
	MPSolver::BasisStatus SiriusInterface::xformBasisStatus(char sirius_basis_status) {
		switch (sirius_basis_status) {
			case SRS_AT_LOWER:
				return MPSolver::AT_LOWER_BOUND;
			case SRS_BASIC:
				return MPSolver::BASIC;
			case SRS_AT_UPPER:
				return MPSolver::AT_UPPER_BOUND;
			case SRS_FREE_LOWER:
			case SRS_FREE_UPPER:
			case SRS_FREE_ZERO:
			case SRS_BASIC_FREE:
				return MPSolver::FREE;
			default:
				LOG(DFATAL) << "Unknown SIRIUS basis status";
				return MPSolver::FREE;
		}
	}

	// Returns the basis status of a row.
	MPSolver::BasisStatus SiriusInterface::row_status(int constraint_index) const {
		if (mMip) {
			LOG(FATAL) << "Basis status only available for continuous problems";
			return MPSolver::FREE;
		}

		if (CheckSolutionIsSynchronized()) {
			if (!mRstat) {
				int const rows = SRSgetnbrows(mLp);
				unique_ptr<char[]> data(new char[rows]);
				char * ptrToData = data.get();
				mRstat.swap(data);
				CHECK_STATUS(SRSgetrowbasisstatus(mLp, &ptrToData));
			}
		}
		else
			mRstat = 0;

		if (mRstat) {
			return xformBasisStatus(mRstat[constraint_index]);
		}
		else {
			LOG(FATAL) << "Row basis status not available";
			return MPSolver::FREE;
		}
	}

	// Returns the basis status of a column.
	MPSolver::BasisStatus SiriusInterface::column_status(int variable_index) const {
		if (mMip) {
			LOG(FATAL) << "Basis status only available for continuous problems";
			return MPSolver::FREE;
		}

		if (CheckSolutionIsSynchronized()) {
			if (!mCstat) {
				int const cols = SRSgetnbcols(mLp);
				unique_ptr<char[]> data(new char[cols]);
				char * ptrToData = data.get();
				mCstat.swap(data);
				CHECK_STATUS(SRSgetcolbasisstatus(mLp, &ptrToData));
			}
		}
		else
			mCstat = 0;

		if (mCstat) {
			return xformBasisStatus(mCstat[variable_index]);
		}
		else {
			LOG(FATAL) << "Column basis status not available";
			return MPSolver::FREE;
		}
	}

	// Extract all variables that have not yet been extracted.
	void SiriusInterface::ExtractNewVariables() {
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
			unique_ptr<int[]> ctype(new int[newcols]);
			unique_ptr<const char *[]> colname(new const char *[newcols]);

			bool have_names = false;
			for (int j = 0, varidx = last_extracted; j < newcols; ++j, ++varidx) {
				MPVariable const *const var = solver_->variables_[varidx];
				lb[j] = var->lb();
				ub[j] = var->ub();
				ctype[j] = var->integer() ? SRS_INTEGER : SRS_CONTINUOUS;
				colname[j] = var->name().empty() ? 0 : var->name().c_str();
				have_names = have_names || var->name().empty();
				obj[j] = solver_->objective_->GetCoefficient(var);
			}

			// Arrays for modifying the problem are setup. Update the index
			// of variables that will get extracted now. Updating indices
			// _before_ the actual extraction makes things much simpler in
			// case we support incremental extraction.
			// In case of error we just reset the indeces.
			std::vector<MPVariable *> const &variables = solver_->variables();
			for (int j = last_extracted; j < var_count; ++j) {
				CHECK(!variable_is_extracted(variables[j]->index()));
				set_variable_as_extracted(variables[j]->index(), true);
			}

			try {
				bool use_newcols = true;

				if (supportIncrementalExtraction) {
					// If we support incremental extraction then we must
					// update existing constraints with the new variables.
					// To do that we use SRSaddcols() to actually create the
					// variables. This is supposed to be faster than combining
					// SRSnewcols() and SRSchgcoeflist().

					// For each column count the size of the intersection with
					// existing constraints.
					unique_ptr<int[]> collen(new int[newcols]);
					for (int j = 0; j < newcols; ++j) collen[j] = 0;
					int nonzeros = 0;
					// TODO: Use a bitarray to flag the constraints that actually
					//       intersect new variables?
					for (int i = 0; i < last_constraint_index_; ++i) {
						MPConstraint const *const ct = solver_->constraints_[i];
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
						// SRSaddcols().
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
						//   so that it is the correct input argument for SRSaddcols
						int *cmatbeg = begin.get();
						cmatbeg[0] = 0;
						cmatbeg[1] = 0;
						++cmatbeg;
						for (int j = 0; j < newcols; ++j)
							cmatbeg[j + 1] = cmatbeg[j] + collen[j];

						for (int i = 0; i < last_constraint_index_; ++i) {
							MPConstraint const *const ct = solver_->constraints_[i];
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
						//FIXME CHECK_STATUS(SRScreatecols(mLp, newcols, nonzeros, obj.get(), cmatbeg, cmatind.get(), cmatval.get(),lb.get(), ub.get()));
					}
				}

				if (use_newcols) {
					// Either incremental extraction is not supported or none of
					// the new variables did intersect an existing constraint.
					// We can just use SRSnewcols() to create the new variables.
					std::vector<int> collen(newcols, 0);
					std::vector<int> cmatbeg(newcols, 0);
					unique_ptr<int[]> cmatind(new int[1]);
					unique_ptr<double[]> cmatval(new double[1]);
					cmatind[0] = 0;
					cmatval[0] = 1.0;

					CHECK_STATUS(
						SRScreatecols(mLp, newcols, obj.get(), ctype.get(), lb.get(), ub.get(), NULL)
					);
					//int const cols = SRSgetnbcols(mLp);
					//unique_ptr<int[]> ind(new int[newcols]);
					//for (int j = 0; j < cols; ++j)
					//	ind[j] = j;
					//CHECK_STATUS(
					//	SRSchgcoltype(mLp, cols - last_extracted, ind.get(), ctype.get())
					//);
				}
				else {
					// Incremental extraction: we must update the ctype of the
					// newly created variables (SRSaddcols() does not allow
					// specifying the ctype)
					if (mMip && SRSgetnbcols(mLp) > 0) {
						// Query the actual number of columns in case we did not
						// manage to extract all columns.
						int const cols = SRSgetnbcols(mLp);
						unique_ptr<int[]> ind(new int[newcols]);
						for (int j = last_extracted; j < cols; ++j)
							ind[j - last_extracted] = j;
						//FIXME CHECK_STATUS(SRSchgcoltype(mLp, cols - last_extracted, ind.get(),ctype.get()));
					}
				}
			}
			catch (...) {
				// Undo all changes in case of error.
				int const cols = SRSgetnbcols(mLp);
				if (cols > last_extracted)
				{
					std::vector<int> colsToDelete;
					for (int i = last_extracted; i < cols; ++i)
						colsToDelete.push_back(i);
					//FIXME (void)SRSdelcols(mLp, colsToDelete.size(), colsToDelete.data());
				}
				std::vector<MPVariable *> const &variables = solver_->variables();
				int const size = variables.size();
				for (int j = last_extracted; j < size; ++j)
					set_variable_as_extracted(j, false);
				throw;
			}
		}
	}

	// Extract constraints that have not yet been extracted.
	void SiriusInterface::ExtractNewConstraints() {
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
			int const cols = SRSgetnbcols(mLp);
			//DCHECK_EQ(last_variable_index_, cols);

			// Update indices of new constraints _before_ actually extracting
			// them. In case of error we will just reset the indices.
			for (int c = offset; c < total; ++c)
				set_constraint_as_extracted(c, true);

			try {
				int nbTerms = 0;
				for (int c = 0; c < newCons; ++c) {
					MPConstraint const *const ct = solver_->constraints_[offset + c];
					nbTerms += ct->coefficients_.size();
				}
				unique_ptr<int[]> rmatbeg(new int[newCons]);
				unique_ptr<int[]> rmatrownbterms(new int[newCons]);
				unique_ptr<int[]> rmatind(new int[nbTerms]);
				unique_ptr<double[]> rmatval(new double[nbTerms]);
				
				unique_ptr<char[]> sense(new char[newCons]);
				unique_ptr<double[]> rhs(new double[newCons]);
				unique_ptr<char const *[]> name(new char const *[newCons]);
				unique_ptr<double[]> rngval(new double[newCons]);
				unique_ptr<int[]> rngind(new int[newCons]);
				bool haveRanges = false;

				// Loop over the new constraints, collecting rows for up to
				// CHUNK constraints into the arrays so that adding constraints
				// is faster.
				int nextNz = 0;
				for (int c = 0; c < newCons; ++c) {
					// Collect up to CHUNK constraints into the arrays.
					MPConstraint const *const ct = solver_->constraints_[offset + c];

					// Setup right-hand side of constraint.
					MakeRhs(ct->lb(), ct->ub(), rhs[c], sense[c],
						rngval[c]);
					haveRanges = haveRanges || (rngval[c] != 0.0);
					rngind[c] = offset + c;

					// Setup left-hand side of constraint.
					rmatbeg[c] = nextNz;
					//const auto& coeffs = ct->coefficients_;
					const auto& coeffs = fixedOrderCoefficientsPerConstraint[ct->index()];
					for (auto it(coeffs.begin()); it != coeffs.end(); ++it) {
						int const idxVar = it->first;
						if (variable_is_extracted(idxVar)) {
							//DCHECK_LT(nextNz, cols);
							//DCHECK_LT(idxVar, cols);
							rmatind[nextNz] = idxVar;
							rmatval[nextNz] = it->second;
							++nextNz;
						}
					}
					rmatrownbterms[c] = nextNz - rmatbeg[c];
					//std::cout
					//	<< c << " "
					//	<< rmatbeg[c] << " "
					//	<< rmatrownbterms[c] << " "
					//	<< rmatind[c] << " "
					//	<< rmatval[c] << " "
					//	<< std::endl;

					// Finally the name of the constraint.
					name[c] = ct->name().empty() ? NULL : ct->name().c_str();
				}

				CHECK_STATUS(
					//SRSaddrows(mLp, nextRow, nextNz, sense.get(), rhs.get(), rngval.get(), rmatbeg.get(), rmatind.get(), rmatval.get())
					SRScreaterows(mLp, newCons, rhs.get(), rngval.get(), sense.get(), name.get())
				);
				SRSsetcoefs(mLp, rmatbeg.get(), rmatrownbterms.get(), rmatind.get(), rmatval.get());
			}
			catch (...) {
				// Undo all changes in case of error.
				int const rows = SRSgetnbrows(mLp);
				std::vector<int> rowsToDelete;
				for (int i = offset; i < rows; ++i)
					rowsToDelete.push_back(i);
				//FIXME if (rows > offset) (void)SRSdelrows(mLp, rowsToDelete.size(), rowsToDelete.data());
				std::vector<MPConstraint *> const &constraints = solver_->constraints();
				int const size = constraints.size();
				for (int i = offset; i < size; ++i) set_constraint_as_extracted(i, false);
				throw;
			}
		}
	}

	// Extract the objective function.
	void SiriusInterface::ExtractObjective() {
		// NOTE: The code assumes that the objective expression does not contain
		//       any non-zero duplicates.

		int const cols = SRSgetnbcols(mLp);
		//DCHECK_EQ(last_variable_index_, cols);

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
				//DCHECK_LT(idx, cols);
				val[idx] = it->second;
			}
		}

		CHECK_STATUS(SRSchgobj(mLp, cols, ind.get(), val.get()));
		//FIXME CHECK_STATUS(SRSsetobjoffset(mLp, solver_->Objective().offset()));
	}

	// ------ Parameters  -----

	// WIP : Use SetSolverSpecificParametersAsString to pass TypeDeBorneDeLaVariable
	bool SiriusInterface::SetSolverSpecificParametersAsString(const std::string& parameters)
	{
		std::stringstream ss(parameters);
		std::string paramName;
		std::getline(ss, paramName, ' ');
		if (paramName == "VAR_BOUNDS_TYPE") {
			std::string paramValue;
			varBoundsTypeValues.clear();
			while (std::getline(ss, paramValue, ' ')) {
				varBoundsTypeValues.push_back(std::stoi(paramValue));
			}
			return true;
		}
		else {
			// unknow paramName
			return false;
		}
	}

	void SiriusInterface::SetParameters(const MPSolverParameters &param) {
		SetCommonParameters(param);
		if (mMip) SetMIPParameters(param);
	}

	void SiriusInterface::SetRelativeMipGap(double value) {
		if (mMip) {
			LOG(WARNING) << "SetRelativeMipGap not implemented for sirius_interface";
			//FIXME CHECK_STATUS(SRSsetdblcontrol(mLp, SRS_MIPRELSTOP, value));
		}
		else {
			LOG(WARNING) << "The relative MIP gap is only available "
				<< "for discrete problems.";
		}
	}

	void SiriusInterface::SetPrimalTolerance(double value) {
		LOG(WARNING) << "SetPrimalTolerance not implemented for sirius_interface";
		//FIXME CHECK_STATUS(SRSsetdblcontrol(mLp, SRS_FEASTOL, value));
	}

	void SiriusInterface::SetDualTolerance(double value) {
		LOG(WARNING) << "SetDualTolerance not implemented for sirius_interface";
		//FIXME CHECK_STATUS(SRSsetdblcontrol(mLp, SRS_OPTIMALITYTOL, value));
	}

	void SiriusInterface::SetPresolveMode(int value) {
		MPSolverParameters::PresolveValues const presolve =
			static_cast<MPSolverParameters::PresolveValues>(value);

		switch (presolve) {
		case MPSolverParameters::PRESOLVE_OFF:
			SRSsetintparams(mLp, SRS_PARAM_PRESOLVE, 0);
			return;
		case MPSolverParameters::PRESOLVE_ON:
			SRSsetintparams(mLp, SRS_PARAM_PRESOLVE, 1);
			return;
		}
		SetIntegerParamToUnsupportedValue(MPSolverParameters::PRESOLVE, value);
	}

	// Sets the scaling mode.
	void SiriusInterface::SetScalingMode(int value) {
		MPSolverParameters::ScalingValues const scaling =
			static_cast<MPSolverParameters::ScalingValues>(value);

		switch (scaling) {
		case MPSolverParameters::SCALING_OFF:
			LOG(WARNING) << "SetScalingMode not implemented for sirius_interface";
			//FIXME CHECK_STATUS(SRSsetintcontrol(mLp, SRS_SCALING, 0));
			break;
		case MPSolverParameters::SCALING_ON:
			LOG(WARNING) << "SetScalingMode not implemented for sirius_interface";
			//FIXME CHECK_STATUS(SRSsetintcontrol(mLp, SRS_SCALING, 1));
			break;
		}
	}

	// Sets the LP algorithm : primal, dual or barrier. Note that SIRIUS offers other
	// LP algorithm (e.g. network) and automatic selection
	void SiriusInterface::SetLpAlgorithm(int value) {
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

		if (alg == 1)
			SetIntegerParamToUnsupportedValue(MPSolverParameters::LP_ALGORITHM, value);
		else {
			LOG(WARNING) << "SetLpAlgorithm not implemented for sirius_interface";
			//FIXME CHECK_STATUS(SRSsetintcontrol(mLp, SRS_DEFAULTALG, alg));
		}
	}

	bool SiriusInterface::ReadParameterFile(std::string const &filename) {
		// Return true on success and false on error.
		return true;
		// LOG(DFATAL) << "ReadParameterFile not implemented for Sirius interface";
		// return false;
	}

	absl::Status SiriusInterface::SetNumThreads(int num_threads)
	{
		// sirius does not support mt
		LOG(WARNING) << "SetNumThreads: sirius does not support multithreading";
		return absl::OkStatus();
	}

	std::string SiriusInterface::ValidFileExtensionForParameterFile() const {
		return ".prm";
	}

	MPSolver::ResultStatus SiriusInterface::Solve(MPSolverParameters const &param) {
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
		case MPSolverParameters::INCREMENTALITY_OFF:
			Reset(); /* This should not be required but re-extracting everything
					  * may be faster, so we do it. */
			break;
		case MPSolverParameters::INCREMENTALITY_ON:
			//FIXME SRSsetintcontrol(mLp, SRS_CRASH, 0);
			break;
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
		CHECK_STATUS(SRSsetintparams(mLp, SRS_PARAM_VERBOSE_SPX, quiet() ? 0 : 1));
		CHECK_STATUS(SRSsetintparams(mLp, SRS_PARAM_VERBOSE_PNE, quiet() ? 0 : 1));

		// Set parameters.
		// NOTE: We must invoke SetSolverSpecificParametersAsString() _first_.
		//       Its current implementation invokes ReadParameterFile() which in
		//       turn invokes SRSreadcopyparam(). The latter will _overwrite_
		//       all current parameter settings in the environment.
		solver_->SetSolverSpecificParametersAsString(solver_->solver_specific_parameter_string_);
		SetParameters(param);
		if (solver_->time_limit()) {
			VLOG(1) << "Setting time limit = " << solver_->time_limit() << " ms.";
			CHECK_STATUS(SRSsetdoubleparams(mLp, SRS_PARAM_MAX_TIME, solver_->time_limit() * 1e-3));
		}

		// Solve.
		// Do not CHECK_STATUS here since some errors (for example CPXERR_NO_MEMORY)
		// still allow us to query useful information.
		timer.Restart();
		int problemStatus = 0;
		if (maximize_) {
			SRSsetintparams(mLp, SRS_PARAM_MAXIMIZE, 1);
		}
		else {
			SRSsetintparams(mLp, SRS_PARAM_MAXIMIZE, 0);
		}

		//std::cout
		//	<< "nbVar " << mLp->problem_mps->NbVar << std::endl
		//	<< "nbRow " << mLp->problem_mps->NbCnt << std::endl;
		//
		//std::cout << "Xmins ";
		//for (int i = 0; i < mLp->problem_mps->NbVar; ++i)
		//	std::cout << mLp->problem_mps->Umin[i] << " ";
		//std::cout << std::endl;
		//
		//std::cout << "Xmaxs ";
		//for (int i = 0; i < mLp->problem_mps->NbVar; ++i)
		//	std::cout << mLp->problem_mps->Umax[i] << " ";
		//std::cout << std::endl;
		//
		//std::cout << "rhs" << std::endl;
		//for (int i = 0; i < mLp->problem_mps->NbCnt; ++i)
		//	std::cout << mLp->problem_mps->SensDeLaContrainte[i] << " " << mLp->problem_mps->Rhs[i] << std::endl;
		//
		//exit(0);

		// set variables's bound's type if any
		if (!varBoundsTypeValues.empty()) {
			SRScopyvarboundstype(mLp, varBoundsTypeValues.data());
		}

		// set solution hints if any
		if (!solver_->solution_hint_.empty()) {
			// store X values
			for (std::pair<const MPVariable*, double>& solution_hint_elt : solver_->solution_hint_) {
				SRSsetxvalue(mLp, solution_hint_elt.first->index(), solution_hint_elt.second);
			}

		}
		if (IsMIP())
			SRSsetintparams(mLp, SRS_FORCE_PNE, 1);

		status = SRSoptimize(mLp);

		if (status) {
			VLOG(1) << absl::StrFormat("Failed to optimize MIP. Error %d", status);
			// NOTE: We do not return immediately since there may be information
			//       to grab (for example an incumbent)
		}
		else {
			VLOG(1) << absl::StrFormat("Solved in %.3f seconds.", timer.Get());
		}

		problemStatus = SRSgetproblemstatus(mLp);
		VLOG(1) << absl::StrFormat("SIRIUS solution status %d.", problemStatus);

		// Figure out what solution we have.
		bool const feasible = !(problemStatus == SRS_STATUS_UNFEASIBLE);

		// Get problem dimensions for solution queries below.
		int const rows = SRSgetnbrows(mLp);
		int const cols = SRSgetnbcols(mLp);
		DCHECK_EQ(rows, solver_->constraints_.size());
		DCHECK_EQ(cols, solver_->variables_.size());

		// Capture objective function value.
		objective_value_ = SRS_NAN;
		if (feasible) {
			(SRSgetobjval(mLp, &objective_value_));
			objective_value_ += solver_->Objective().offset();
		}
		VLOG(1) << "objective = " << objective_value_;

		// Capture primal and dual solutions
		if (mMip) {
			// If there is a primal feasible solution then capture it.
			if (feasible) {
				if (cols > 0) {
					double * x = new double[cols];
					CHECK_STATUS(SRSgetx(mLp, &x));
					for (int i = 0; i < solver_->variables_.size(); ++i) {
						MPVariable *const var = solver_->variables_[i];
						var->set_solution_value(x[i]);
						VLOG(3) << var->name() << ": value =" << x[i];
					}
					delete[] x;
				}
			}
			else {
				for (int i = 0; i < solver_->variables_.size(); ++i)
					solver_->variables_[i]->set_solution_value(SRS_NAN);
			}

			// MIP does not have duals
			for (int i = 0; i < solver_->variables_.size(); ++i)
				solver_->variables_[i]->set_reduced_cost(SRS_NAN);
			unique_ptr<double[]> pi(new double[rows]);
			if (feasible) {
				double * dualValues = pi.get();
				CHECK_STATUS(SRSgetdualvalues(mLp, &dualValues));
			}
			for (int i = 0; i < solver_->constraints_.size(); ++i) {
				MPConstraint *const ct = solver_->constraints_[i];
				bool dual = false;
				if (feasible) {
					ct->set_dual_value(pi[i]);
					dual = true;
				}
				else
					ct->set_dual_value(SRS_NAN);
				VLOG(4) << "row " << ct->index() << ":"
					<< (dual ? absl::StrFormat("  dual = %f", pi[i]) : "");
			}
		}
		else {
			// Continuous problem.
			if (cols > 0) {
				double * dj = new double[cols];
				double * x = new double[cols];
				CHECK_STATUS(SRSgetx(mLp, &x));

				if (feasible) {
					CHECK_STATUS(SRSgetreducedcosts(mLp, &dj));
				}
				for (int i = 0; i < solver_->variables_.size(); ++i) {
					MPVariable *const var = solver_->variables_[i];
					var->set_solution_value(x[i]);
					bool value = false, dual = false;

					if (feasible) {
						var->set_solution_value(x[i]);
						value = true;
					}
					else
						var->set_solution_value(SRS_NAN);
					if (feasible) {
						var->set_reduced_cost(dj[i]);
						dual = true;
					}
					else
						var->set_reduced_cost(SRS_NAN);
					VLOG(3) << var->name() << ":"
						<< (value ? absl::StrFormat("  value = %f", x[i]) : "")
						<< (dual ? absl::StrFormat("  reduced cost = %f", dj[i]) : "");
				}
				delete[] x;
				delete[] dj;
			}

			if (rows > 0) {
				unique_ptr<double[]> pi(new double[rows]);
				if (feasible) {
					double * dualValues = pi.get();
					CHECK_STATUS(SRSgetdualvalues(mLp, &dualValues));
				}
				for (int i = 0; i < solver_->constraints_.size(); ++i) {
					MPConstraint *const ct = solver_->constraints_[i];
					bool dual = false;
					if (feasible) {
						ct->set_dual_value(pi[i]);
						dual = true;
					}
					else
						ct->set_dual_value(SRS_NAN);
					VLOG(4) << "row " << ct->index() << ":"
						<< (dual ? absl::StrFormat("  dual = %f", pi[i]) : "");
				}
			}
		}

		// Map SIRIUS status to more generic solution status in MPSolver
		switch (problemStatus) {
		case SRS_STATUS_OPTIMAL:
			result_status_ = MPSolver::OPTIMAL;
			break;
		case SRS_STATUS_UNFEASIBLE:
			result_status_ = MPSolver::INFEASIBLE;
			break;
		case SRS_STATUS_UNBOUNDED:
			result_status_ = MPSolver::UNBOUNDED;
			break;
		default:
			result_status_ = feasible ? MPSolver::FEASIBLE : MPSolver::ABNORMAL;
			break;
		}

		sync_status_ = SOLUTION_SYNCHRONIZED;
		return result_status_;
	}

	void SiriusInterface::Write(const std::string& filename) {
		if (sync_status_ == MUST_RELOAD) {
			Reset();
		}
		ExtractModel();
		VLOG(1) << "Writing Sirius MPS \"" << filename << "\".";
		const int status = SRSwritempsprob(mLp->problem_mps, filename.c_str());
		if (status) {
			LOG(WARNING) << "Failed to write MPS.";
		}
	}

	MPSolverInterface *BuildSiriusInterface(bool mip, MPSolver *const solver) {
		return new SiriusInterface(solver, mip);
	}

}  // namespace operations_research
#endif  // #if defined(USE_SIRUS)
