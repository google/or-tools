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
// This .swig file is only used in the open-source export of
//
// It exposes the linear programming and integer programming solver.
//
// The C# API is quite different from the C++ API; mostly because it
// supports the same "Natural language" API as the python wrapper. See the
// usage examples.
//
// USAGE EXAMPLES:
// - examples/csharp/cslinearprogramming.cs
// - examples/csharp/csintegerprogramming.cs

%include base/base.swig

%{
#include "linear_solver/linear_solver.h"
#include "linear_solver/linear_solver2.pb.h"
#include "linear_solver/linear_solver_ext.h"
%}

// C# "using" directives needed for the code inlined below.
%typemap(csimports) SWIGTYPE %{
  using System;
  using System.Collections.Generic;
  using System.Runtime.InteropServices;
%}

// Patch the MPVariable class to support the natural language API.
// This code refers to classes ("LinearExpr", etc..) defined in
// src/com/google/ortools/linearsolver/.
%typemap(cscode) operations_research::MPVariable %{
  public static LinearExpr operator+(Variable a, double v)
  {
    return new VarWrapper(a) + v;
  }

  public static LinearExpr operator+(double v, Variable a)
  {
    return a + v;
  }

  public static LinearExpr operator+(Variable a, LinearExpr b)
  {
    return new VarWrapper(a) + b;
  }

  public static LinearExpr operator+(Variable a, Variable b)
  {
    return new VarWrapper(a) + new VarWrapper(b);
  }

  public static LinearExpr operator+(LinearExpr a, Variable b)
  {
    return a + new VarWrapper(b);
  }

  public static LinearExpr operator-(Variable a, double v)
  {
    return new VarWrapper(a) - v;
  }

  public static LinearExpr operator-(double v, Variable a)
  {
    return v - new VarWrapper(a);
  }

  public static LinearExpr operator-(Variable a, LinearExpr b)
  {
    return new VarWrapper(a) - b;
  }

  public static LinearExpr operator-(LinearExpr a, Variable b)
  {
    return a - new VarWrapper(b);
  }

  public static LinearExpr operator-(Variable a, Variable b)
  {
    return new VarWrapper(a) - new VarWrapper(b);
  }

  public static LinearExpr operator-(Variable a)
  {
    return - new VarWrapper(a);
  }

  public static LinearExpr operator*(Variable a, double v)
  {
    return new VarWrapper(a) * v;
  }

  public static LinearExpr operator/(Variable a, double v)
  {
    return new VarWrapper(a) / v;
  }

  public static LinearExpr operator*(double v, Variable a)
  {
    return v * new VarWrapper(a);
  }

  public static RangeConstraint operator==(Variable a, double v)
  {
    return new VarWrapper(a) == v;
  }

  public static RangeConstraint operator==(double v, Variable a)
  {
    return v == new VarWrapper(a);
  }

  public static RangeConstraint operator!=(Variable a, double v)
  {
    return new VarWrapper(a) != v;
  }

  public static RangeConstraint operator!=(double v, Variable a)
  {
    return new VarWrapper(a) != v;
  }

  public static Equality operator==(Variable a, LinearExpr b)
  {
    return new VarWrapper(a) == b;
  }

  public static Equality operator==(LinearExpr a, Variable b)
  {
    return a == new VarWrapper(b);
  }

  public static VarEquality operator==(Variable a, Variable b)
  {
    return new VarEquality(a, b, true);
  }

  public static Equality operator!=(Variable a, LinearExpr b)
  {
    return new VarWrapper(a) != b;
  }

  public static Equality operator!=(LinearExpr a, Variable b)
  {
    return a != new VarWrapper(b);
  }

  public static VarEquality operator!=(Variable a, Variable b)
  {
    return new VarEquality(a, b, false);
  }

  public static RangeConstraint operator<=(Variable a, double v)
  {
    return new VarWrapper(a) <= v;
  }

  public static RangeConstraint operator>=(Variable a, double v)
  {
    return new VarWrapper(a) >= v;
  }

  public static RangeConstraint operator<=(double v, Variable a)
  {
    return new VarWrapper(a) >= v;
  }

  public static RangeConstraint operator>=(double v, Variable a)
  {
    return new VarWrapper(a) <= v;
  }

  public static RangeConstraint operator<=(Variable a, LinearExpr b)
  {
    return new VarWrapper(a) <= b;
  }

  public static RangeConstraint operator>=(Variable a, LinearExpr b)
  {
    return new VarWrapper(a) >= b;
  }

  public static RangeConstraint operator<=(Variable a, Variable b)
  {
    return new VarWrapper(a) <= new VarWrapper(b);
  }

  public static RangeConstraint operator>=(Variable a, Variable b)
  {
    return new VarWrapper(a) >= new VarWrapper(b);
  }

  public static RangeConstraint operator<=(LinearExpr a, Variable b)
  {
    return a <= new VarWrapper(b);
  }

  public static RangeConstraint operator>=(LinearExpr a, Variable b)
  {
    return a >= new VarWrapper(b);
  }
%}

// Patch the MPSolver class to:
// - support custom versions of the array-based APIs (MakeVarArray, etc).
// - customize the construction, and the OptimizationProblemType enum.
// - support the natural language API.
%typemap(cscode) operations_research::MPSolver %{
  public Variable[] MakeVarArray(int count,
                                 double lb,
                                 double ub,
                                 bool integer) {
    Variable[] array = new Variable[count];
    for (int i = 0; i < count; ++i) {
      array[i] = MakeVar(lb, ub, integer, "");
    }
    return array;
  }

  public Variable[] MakeVarArray(int count,
                                 double lb,
                                 double ub,
                                 bool integer,
                                 string var_name) {
    Variable[] array = new Variable[count];
    for (int i = 0; i < count; ++i) {
      array[i] = MakeVar(lb, ub, integer, var_name + i);
    }
    return array;
  }

  public Variable[,] MakeVarMatrix(int rows,
                                   int cols,
                                   double lb,
                                   double ub,
                                   bool integer) {
    Variable[,] matrix = new Variable[rows, cols];
    for (int i = 0; i < rows; ++i) {
      for (int j = 0; j < cols; ++j) {
        matrix[i,j] = MakeVar(lb, ub, integer, "");
      }
    }
    return matrix;
  }

  public Variable[,] MakeVarMatrix(int rows,
                                   int cols,
                                   double lb,
                                   double ub,
                                   bool integer,
                                   string name) {
    Variable[,] matrix = new Variable[rows, cols];
    for (int i = 0; i < rows; ++i) {
      for (int j = 0; j < cols; ++j) {
        string var_name = name + "[" + i + ", " + j +"]";
        matrix[i,j] = MakeVar(lb, ub, integer, var_name);
      }
    }
    return matrix;
  }

  public Variable[] MakeNumVarArray(int count, double lb, double ub) {
    return MakeVarArray(count, lb, ub, false);
  }

  public Variable[] MakeNumVarArray(int count,
                                    double lb,
                                    double ub,
                                    string var_name) {
    return MakeVarArray(count, lb, ub, false, var_name);
  }

  public Variable[,] MakeNumVarMatrix(int rows,
                                      int cols,
                                      double lb,
                                      double ub) {
    Variable[,] matrix = new Variable[rows, cols];
    for (int i = 0; i < rows; ++i) {
      for (int j = 0; j < cols; ++j) {
        matrix[i,j] = MakeNumVar(lb, ub, "");
      }
    }
    return matrix;
  }

  public Variable[,] MakeNumVarMatrix(int rows,
                                      int cols,
                                      double lb,
                                      double ub,
                                      string name) {
    Variable[,] matrix = new Variable[rows, cols];
    for (int i = 0; i < rows; ++i) {
      for (int j = 0; j < cols; ++j) {
        string var_name = name + "[" + i + ", " + j +"]";
        matrix[i,j] = MakeNumVar(lb, ub, var_name);
      }
    }
    return matrix;
  }

  public Variable[] MakeIntVarArray(int count, double lb, double ub) {
    return MakeVarArray(count, lb, ub, true);
  }

  public Variable[] MakeIntVarArray(int count,
                                    double lb,
                                    double ub,
                                    string var_name) {
    return MakeVarArray(count, lb, ub, true, var_name);
  }

  public Variable[,] MakeIntVarMatrix(int rows,
                                      int cols,
                                      double lb,
                                      double ub) {
    Variable[,] matrix = new Variable[rows, cols];
    for (int i = 0; i < rows; ++i) {
      for (int j = 0; j < cols; ++j) {
        matrix[i,j] = MakeIntVar(lb, ub, "");
      }
    }
    return matrix;
  }

  public Variable[,] MakeIntVarMatrix(int rows,
                                      int cols,
                                      double lb,
                                      double ub,
                                      string name) {
    Variable[,] matrix = new Variable[rows, cols];
    for (int i = 0; i < rows; ++i) {
      for (int j = 0; j < cols; ++j) {
        string var_name = name + "[" + i + ", " + j +"]";
        matrix[i,j] = MakeIntVar(lb, ub, var_name);
      }
    }
    return matrix;
  }

  public Variable[] MakeBoolVarArray(int count) {
    return MakeVarArray(count, 0.0, 1.0, true);
  }

  public Variable[] MakeBoolVarArray(int count, string var_name) {
    return MakeVarArray(count, 0.0, 1.0, true, var_name);
  }

  public Variable[,] MakeBoolVarMatrix(int rows, int cols) {
    Variable[,] matrix = new Variable[rows, cols];
    for (int i = 0; i < rows; ++i) {
      for (int j = 0; j < cols; ++j) {
        matrix[i,j] = MakeBoolVar("");
      }
    }
    return matrix;
  }

  public Variable[,] MakeBoolVarMatrix(int rows, int cols, string name) {
    Variable[,] matrix = new Variable[rows, cols];
    for (int i = 0; i < rows; ++i) {
      for (int j = 0; j < cols; ++j) {
        string var_name = name + "[" + i + ", " + j +"]";
        matrix[i,j] = MakeBoolVar(var_name);
      }
    }
    return matrix;
  }

  public static int GetSolverEnum(String solverType) {
    System.Reflection.FieldInfo fieldInfo =
      typeof(Solver).GetField(solverType);
    if (fieldInfo != null) {
      return (int)fieldInfo.GetValue(null);
    } else {
      throw new System.ApplicationException("Solver not supported");
    }
  }

  public static Solver CreateSolver(String name, String type) {
    System.Reflection.FieldInfo fieldInfo =
        typeof(Solver).GetField(type);
    if (fieldInfo != null) {
      return new Solver(name, (int)fieldInfo.GetValue(null));
    } else {
      return null;
    }
  }

  public Constraint Add(LinearConstraint constraint) {
    return constraint.Extract(this);
  }

  public void Minimize(LinearExpr expr)
  {
    Objective().Clear();
    Objective().SetMinimization();
    Dictionary<Variable, double> coefficients =
        new Dictionary<Variable, double>();
    double constant = expr.Visit(coefficients);
    foreach (KeyValuePair<Variable, double> pair in coefficients)
    {
      Objective().SetCoefficient(pair.Key, pair.Value);
    }
    Objective().SetOffset(constant);
  }

  public void Maximize(LinearExpr expr)
  {
    Objective().Clear();
    Objective().SetMaximization();
    Dictionary<Variable, double> coefficients =
        new Dictionary<Variable, double>();
    double constant = expr.Visit(coefficients);
    foreach (KeyValuePair<Variable, double> pair in coefficients)
    {
      Objective().SetCoefficient(pair.Key, pair.Value);
    }
    Objective().SetOffset(constant);
  }

  public void Minimize(Variable var)
  {
    Objective().Clear();
    Objective().SetMinimization();
    Objective().SetCoefficient(var, 1.0);
  }

  public void Maximize(Variable var)
  {
    Objective().Clear();
    Objective().SetMaximization();
    Objective().SetCoefficient(var, 1.0);
  }
%}

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
%rename (MakeConstraint) operations_research::MPSolver::MakeRowConstraint;
%rename (Objective) operations_research::MPSolver::MutableObjective;

// Expose the MPSolver's basic API, with trivial renames when needed.
%unignore operations_research::MPSolver::MPSolver;
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
%unignore operations_research::MPSolver::ComputeExactConditionNumber;
%rename (Nodes) operations_research::MPSolver::nodes;
%rename (Iterations) operations_research::MPSolver::iterations;
%unignore operations_research::MPSolver::BasisStatus;
%unignore operations_research::MPSolver::FREE;
%unignore operations_research::MPSolver::AT_LOWER_BOUND;
%unignore operations_research::MPSolver::AT_UPPER_BOUND;
%unignore operations_research::MPSolver::FIXED_VALUE;
%unignore operations_research::MPSolver::BASIC;

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
%rename (SetIsLazy) operations_research::MPConstraint::set_is_lazy;

// MPConstraint: reader API.
%unignore operations_research::MPConstraint::GetCoefficient;
%rename (Lb) operations_research::MPConstraint::lb;
%rename (Ub) operations_research::MPConstraint::ub;
%rename (Name) operations_research::MPConstraint::name;
%rename (Activity) operations_research::MPConstraint::activity;
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
%unignore operations_research::MPSolverParameters::DoubleParam;
%unignore operations_research::MPSolverParameters::RELATIVE_MIP_GAP;
%unignore operations_research::MPSolverParameters::GetDoubleParam;
%unignore operations_research::MPSolverParameters::SetDoubleParam;
%unignore operations_research::MPSolverParameters::kDefaultPrimalTolerance;

// We want to ignore the CoeffMap class; but since it inherits from some
// hash_map<>, swig complains about an undefined base class. Silence it.
%warnfilter(401) CoeffMap;

%include "linear_solver/linear_solver.h"
%include "linear_solver/linear_solver_ext.h"

%unignoreall
