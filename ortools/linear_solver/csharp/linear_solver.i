// Copyright 2010-2014 Google
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

// This .i file is only used in the open-source export of
// util/operations_research (github or-tools)
//
// It exposes the linear programming and integer programming solver.
//
// The C# API is quite different from the C++ API; mostly because it
// supports the same "Natural language" API as the python wrapper. See the
// usage examples.
//
// This .i file is complemented by C# extensions (using partial classes)
// of some wrapped C++ classes, like Solver or Variable. There are also
// other C# helper classes. See src/com/google/ortools/linearsolver.
//
// USAGE EXAMPLES:
// - examples/csharp/cslinearprogramming.cs
// - examples/csharp/csintegerprogramming.cs

%include "ortools/base/base.i"
%include "std_vector.i"

%{
#include "ortools/linear_solver/linear_solver.h"
#include "ortools/linear_solver/linear_solver.pb.h"
%}

// We need to forward-declare the proto here, so that the PROTO_* macros
// involving them work correctly. The order matters very much: this declaration
// needs to be before the %{ #include ".../linear_solver.h" %}.
namespace operations_research {
class MPModelProto;
class MPModelRequest;
class MPSolutionResponse;
}  // namespace operations_research

// Allow partial C# classes. That way, we can put our C# code extension in C#
// files instead of using typemap(cscode).
/* allow partial c# classes */
%typemap(csclassmodifiers) SWIGTYPE "public partial class"

%typemap(csclassmodifiers) operations_research::MPVariable "public partial class"
%typemap(csclassmodifiers) operations_research::MPSolver "public partial class"

%template(MpDoubleVector) std::vector<double>;

%ignoreall

%unignore operations_research;

// Rename all the exposed classes, by removing the "MP" prefix.
%rename (Solver) operations_research::MPSolver;
%rename (Variable) operations_research::MPVariable;
%rename (Constraint) operations_research::MPConstraint;
%rename (Objective) operations_research::MPObjective;
%rename (SolverParameters) operations_research::MPSolverParameters;

// Expose the MPSolver::OptimizationProblemType enum.
%unignore operations_research::MPSolver::OptimizationProblemType;
%unignore operations_research::MPSolver::CLP_LINEAR_PROGRAMMING;
%unignore operations_research::MPSolver::GLOP_LINEAR_PROGRAMMING;
%unignore operations_research::MPSolver::GLPK_LINEAR_PROGRAMMING;
%unignore operations_research::MPSolver::SCIP_MIXED_INTEGER_PROGRAMMING;
%unignore operations_research::MPSolver::CBC_MIXED_INTEGER_PROGRAMMING;
%unignore operations_research::MPSolver::GLPK_MIXED_INTEGER_PROGRAMMING;
%unignore operations_research::MPSolver::GUROBI_LINEAR_PROGRAMMING;
%unignore operations_research::MPSolver::GUROBI_MIXED_INTEGER_PROGRAMMING;
%unignore operations_research::MPSolver::SULUM_LINEAR_PROGRAMMING;
%unignore operations_research::MPSolver::SULUM_MIXED_INTEGER_PROGRAMMING;

// Expose the MPSolver::ResultStatus enum.
%unignore operations_research::MPSolver::ResultStatus;
%unignore operations_research::MPSolver::OPTIMAL;
%unignore operations_research::MPSolver::FEASIBLE;
%unignore operations_research::MPSolver::INFEASIBLE;
%unignore operations_research::MPSolver::UNBOUNDED;
%unignore operations_research::MPSolver::ABNORMAL;
%unignore operations_research::MPSolver::NOT_SOLVED;

// Expose the MPSolver's basic API, with some non-trivial renames.
// We intentionally don't expose MakeRowConstraint(LinearExpr), because this
// "natural language" API is specific to C++: other languages may add their own
// syntactic sugar on top of MPSolver instead of this.
%rename (MakeConstraint) operations_research::MPSolver::MakeRowConstraint(double, double);
%rename (MakeConstraint) operations_research::MPSolver::MakeRowConstraint();
%rename (MakeConstraint) operations_research::MPSolver::MakeRowConstraint(double, double, const std::string&);
%rename (MakeConstraint) operations_research::MPSolver::MakeRowConstraint(const std::string&);

%rename (Objective) operations_research::MPSolver::MutableObjective;

// Expose the MPSolver's basic API, with trivial renames when needed.
%unignore operations_research::MPSolver::MPSolver;
%unignore operations_research::MPSolver::~MPSolver;
%unignore operations_research::MPSolver::MakeBoolVar;
%unignore operations_research::MPSolver::MakeIntVar;
%unignore operations_research::MPSolver::MakeNumVar;
%unignore operations_research::MPSolver::MakeVar;
%unignore operations_research::MPSolver::Solve;
%unignore operations_research::MPSolver::VerifySolution;
%unignore operations_research::MPSolver::Reset;
%rename (SetTimeLimit) operations_research::MPSolver::set_time_limit;

// Expose some of the more advanced MPSolver API.
%unignore operations_research::MPSolver::SupportsProblemType;
%unignore operations_research::MPSolver::SetSolverSpecificParametersAsString;
%rename (WallTime) operations_research::MPSolver::wall_time;
%unignore operations_research::MPSolver::Clear;
%unignore operations_research::MPSolver::NumVariables;
%unignore operations_research::MPSolver::NumConstraints;
%unignore operations_research::MPSolver::EnableOutput;
%unignore operations_research::MPSolver::SuppressOutput;
%unignore operations_research::MPSolver::LookupConstraintOrNull;
%unignore operations_research::MPSolver::LookupVariableOrNull;

// Expose very advanced parts of the MPSolver API. For expert users only.
%unignore operations_research::MPSolver::ComputeConstraintActivities;
%unignore operations_research::MPSolver::ComputeExactConditionNumber;
%rename (Nodes) operations_research::MPSolver::nodes;
%rename (Iterations) operations_research::MPSolver::iterations;
%unignore operations_research::MPSolver::BasisStatus;
%unignore operations_research::MPSolver::FREE;
%unignore operations_research::MPSolver::AT_LOWER_BOUND;
%unignore operations_research::MPSolver::AT_UPPER_BOUND;
%unignore operations_research::MPSolver::FIXED_VALUE;
%unignore operations_research::MPSolver::BASIC;

// Extend code.
%unignore operations_research::MPSolver::ExportModelAsLpFormat(bool);
%unignore operations_research::MPSolver::ExportModelAsMpsFormat(bool, bool);
%extend operations_research::MPSolver {
  std::string ExportModelAsLpFormat(bool obfuscated) {
    std::string output;
    if (!$self->ExportModelAsLpFormat(obfuscated, &output)) return "";
    return output;
  }

  std::string ExportModelAsMpsFormat(bool fixed_format, bool obfuscated) {
    std::string output;
    if (!$self->ExportModelAsMpsFormat(fixed_format, obfuscated, &output)) {
      return "";
    }
    return output;
  }
}

// MPVariable: writer API.
%unignore operations_research::MPVariable::SetInteger;
%rename (SetLb) operations_research::MPVariable::SetLB;
%rename (SetUb) operations_research::MPVariable::SetUB;
%unignore operations_research::MPVariable::SetBounds;

// MPVariable: reader API.
%rename (SolutionValue) operations_research::MPVariable::solution_value;
%rename (Lb) operations_research::MPVariable::lb;
%rename (Ub) operations_research::MPVariable::ub;
%rename (Name) operations_research::MPVariable::name;
%rename (BasisStatus) operations_research::MPVariable::basis_status;
%rename (ReducedCost) operations_research::MPVariable::reduced_cost;  // expert

// MPConstraint: writer API.
%unignore operations_research::MPConstraint::SetCoefficient;
%rename (SetLb) operations_research::MPConstraint::SetLB;
%rename (SetUb) operations_research::MPConstraint::SetUB;
%unignore operations_research::MPConstraint::SetBounds;
%rename (Index) operations_research::MPConstraint::index;
%rename (SetIsLazy) operations_research::MPConstraint::set_is_lazy;

// MPConstraint: reader API.
%unignore operations_research::MPConstraint::GetCoefficient;
%rename (Lb) operations_research::MPConstraint::lb;
%rename (Ub) operations_research::MPConstraint::ub;
%rename (Name) operations_research::MPConstraint::name;
%rename (BasisStatus) operations_research::MPConstraint::basis_status;
%rename (DualValue) operations_research::MPConstraint::dual_value;  // expert
%rename (IsLazy) operations_research::MPConstraint::is_lazy;  // expert

// MPObjective: writer API.
%unignore operations_research::MPObjective::SetCoefficient;
%unignore operations_research::MPObjective::SetMinimization;
%unignore operations_research::MPObjective::SetMaximization;
%unignore operations_research::MPObjective::SetOptimizationDirection;
%unignore operations_research::MPObjective::Clear;
%unignore operations_research::MPObjective::SetOffset;
%unignore operations_research::MPObjective::AddOffset;

// MPObjective: reader API.
%unignore operations_research::MPObjective::Value;
%unignore operations_research::MPObjective::GetCoefficient;
%rename (Minimization) operations_research::MPObjective::minimization;
%rename (Maximization) operations_research::MPObjective::maximization;
%rename (Offset) operations_research::MPObjective::offset;
%unignore operations_research::MPObjective::BestBound;

// MPSolverParameters API. For expert users only.
%unignore operations_research::MPSolverParameters;
%unignore operations_research::MPSolverParameters::MPSolverParameters;
%unignore operations_research::MPSolverParameters::DoubleParam;
%unignore operations_research::MPSolverParameters::RELATIVE_MIP_GAP;
%unignore operations_research::MPSolverParameters::GetDoubleParam;
%unignore operations_research::MPSolverParameters::SetDoubleParam;
%unignore operations_research::MPSolverParameters::kDefaultPrimalTolerance;

// We want to ignore the CoeffMap class; but since it inherits from some
// std::unordered_map<>, swig complains about an undefined base class. Silence it.
%warnfilter(401) CoeffMap;

%include "ortools/linear_solver/linear_solver.h"

%unignoreall
