// TODO(user): Refactor this file to adhere to the SWIG style guide.

%include base/base.swig
%include util/java/data.swig

// Swig doesn't like module initializers.
#define DECLARE_MODULE_INITIALIZER(x);

// Include the file we want to wrap a first time.
%{
#include "linear_solver/linear_solver.h"
#include "linear_solver/linear_solver2.pb.h"
#include "linear_solver/linear_solver_ext.h"
%}

%module(directors="1") operations_research;

%typemap(javaimports) SWIGTYPE %{
import java.lang.reflect.*;
%}

namespace operations_research {
// Rename rules on MPVariable.
%rename (basisStatus) MPVariable::basis_status;
%rename (reducedCost) MPVariable::reduced_cost;
%rename (setBounds) MPVariable::SetBounds;
%rename (setInteger) MPVariable::SetInteger;
%rename (setLb) MPVariable::SetLB;
%rename (setUb) MPVariable::SetUB;
%rename (solutionValue) MPVariable::solution_value;

// Rename rules on MPConstraint.
%rename (basisStatus) MPConstraint::basis_status;
%rename (dualValue) MPConstraint::dual_value;
%rename (getCoefficient) MPConstraint::GetCoefficient;
%rename (setBounds) MPConstraint::SetBounds;
%rename (setCoefficient) MPConstraint::SetCoefficient;
%rename (setLb) MPConstraint::SetLB;
%rename (setUb) MPConstraint::SetUB;
%rename (setIsLazy) MPConstraint::set_is_lazy;
%rename (isLazy) MPConstraint::is_lazy;

// Rename rules on MPObjective.
%rename (addOffset) MPObjective::AddOffset;
%rename (bestBound) MPObjective::BestBound;
%rename (clear) MPObjective::Clear;
%rename (getCoefficient) MPObjective::GetCoefficient;
%rename (setCoefficient) MPObjective::SetCoefficient;
%rename (setMaximization) MPObjective::SetMaximization;
%rename (setMinimization) MPObjective::SetMinimization;
%rename (setOffset) MPObjective::SetOffset;
%rename (setOptimizationDirection) MPObjective::SetOptimizationDirection;
%rename (value) MPObjective::Value;

// Rename rules on MPSolverParameters.
%rename (getDoubleParam) MPSolverParameters::GetDoubleParam;
%rename (getIntegerParam) MPSolverParameters::GetIntegerParam;
%rename (reset) MPSolverParameters::Reset;
%rename (resetDoubleParam) MPSolverParameters::ResetDoubleParam;
%rename (resetIntegerParam) MPSolverParameters::ResetIntegerParam;
%rename (setDoubleParam) MPSolverParameters::SetDoubleParam;
%rename (setIntegerParam) MPSolverParameters::SetIntegerParam;

// Rename rules on MPSolver.
%rename (checkAllNamesValidity) MPSolver::CheckAllNamesValidity;
%rename (checkNameValidity) MPSolver::CheckNameValidity;
%rename (clear) MPConstraint::Clear;
%rename (clear) MPObjective::Clear;
%rename (clear) MPSolver::Clear;
%rename (computeExactConditionNumber) MPSolver::ComputeExactConditionNumber;
%rename (loadModelFromProto) MPSolver::LoadModelFromProto;
%rename (lookupVariableOrNull) MPSolver::LookupVariableOrNull;
%rename (lookupConstraintOrNull) MPSolver::LookupConstraintOrNull;
%rename (makeBoolVar) MPSolver::MakeBoolVar;
%rename (makeIntVar) MPSolver::MakeIntVar;
%rename (makeNumVar) MPSolver::MakeNumVar;
%rename (makeConstraint) MPSolver::MakeRowConstraint;
%rename (makeVar) MPSolver::MakeVar;
%rename (minimization) MPSolver::Minimization;
%rename (numConstraints) MPSolver::NumConstraints;
%rename (numVariables) MPSolver::NumVariables;
%rename (objective) MPSolver::MutableObjective;
%rename (reset) MPSolver::Reset;
%rename (setMaximization) MPObjective::SetMaximization();
%rename (setMinimization) MPObjective::SetMinimization();
%rename (setCoefficient) MPObjective::SetCoefficient;
%rename (setOffset) MPObjective::SetOffset;
%rename (setOptimizationDirection) MPObjective::SetOptimizationDirection;
%rename (setTimeLimit) MPSolver::set_time_limit;
%rename (setWriteModelFilename) MPSolver::set_write_model_filename;
%rename (solve) MPSolver::Solve;
%rename (solverVersion) MPSolver::SolverVersion;
%rename (suppressOutput) MPSolver::SuppressOutput;
%rename (timeLimit) MPSolver::time_limit;
%rename (wallTime) MPSolver::wall_time;
%rename (writeModelFilename) MPSolver::write_model_filename;
// Ignore non-mutable version of the objective accessor.
%ignore MPSolver::Objective;
// Ignore Make*VarArray: see replacement java code below.
%ignore MPSolver::MakeVarArray;
%ignore MPSolver::MakeNumVarArray;
%ignore MPSolver::MakeIntVarArray;
%ignore MPSolver::MakeBoolVarArray;
// The following 6 methods that use protocol buffers as output
// arguments are replaced by methods that return a protocol buffer,
// see code below.
%ignore MPSolver::ExportModelToNewProto;
%ignore MPSolver::FillSolutionResponseProto;
%ignore MPSolver::SolveWithProto;
%ignore MPSolver::ExportModel;
%ignore MPSolver::FillSolutionResponse;
%ignore MPSolver::SolveWithProtocolBuffers;
// Ignore export to string methods.
%ignore operations_research::MPSolver::ExportModelAsLpFormat;
%ignore operations_research::MPSolver::ExportModelAsMpsFormat;
// Access to the underlying solver is available only in C++.
%ignore MPSolver::underlying_solver;


// Add java code on MPSolver.
%typemap(javacode) MPSolver %{
  public MPVariable[] makeVarArray(int count,
                                   double lb,
                                   double ub,
                                   boolean integer) {
    MPVariable[] array = new MPVariable[count];
    for (int i = 0; i < count; ++i) {
      array[i] = makeVar(lb, ub, integer, "");
    }
    return array;
  }

  public MPVariable[] makeVarArray(int count,
                                   double lb,
                                   double ub,
                                   boolean integer,
                                   String var_name) {
    MPVariable[] array = new MPVariable[count];
    for (int i = 0; i < count; ++i) {
      array[i] = makeVar(lb, ub, integer, var_name + i);
    }
    return array;
  }

  public MPVariable[] makeNumVarArray(int count, double lb, double ub) {
    return makeVarArray(count, lb, ub, false);
  }

  public MPVariable[] makeNumVarArray(int count,
                                      double lb,
                                      double ub,
                                      String var_name) {
    return makeVarArray(count, lb, ub, false, var_name);
  }

  public MPVariable[] makeIntVarArray(int count, double lb, double ub) {
    return makeVarArray(count, lb, ub, true);
  }

  public MPVariable[] makeIntVarArray(int count,
                                      double lb,
                                      double ub,
                                      String var_name) {
    return makeVarArray(count, lb, ub, true, var_name);
  }

  public MPVariable[] makeBoolVarArray(int count) {
    return makeVarArray(count, 0.0, 1.0, true);
  }

  public MPVariable[] makeBoolVarArray(int count, String var_name) {
    return makeVarArray(count, 0.0, 1.0, true, var_name);
  }

  public static final int getSolverEnum(String solver_name)
      throws java.lang.ClassNotFoundException,
      java.lang.NoSuchFieldException,
      java.lang.IllegalAccessException {
Class c = Class.forName("com.google.ortools.linearsolver.MPSolver");
    Field field = c.getField(solver_name);
    return field.getInt(null);
  }
%}

%extend MPSolver {
  std::string exportModelAsLpFormat(bool obfuscated) {
    std::string output;
    if (!self->ExportModelAsLpFormat(obfuscated, &output)) return "";
    return output;
  }

  std::string exportModelAsMpsFormat(bool fixed_format, bool obfuscated) {
    std::string output;
    if (!self->ExportModelAsMpsFormat(fixed_format, obfuscated, &output)) {
      return "";
    }
    return output;
  }
}
}  // namespace operations_research

// Wrap linear_solver includes
%include "linear_solver/linear_solver.h"
