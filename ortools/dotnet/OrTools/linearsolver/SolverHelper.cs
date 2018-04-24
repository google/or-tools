// Copyright 2010-2017 Google
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

namespace Google.OrTools.LinearSolver {
using System;
using System.Collections.Generic;

// Patch the MPSolver class to:
// - support custom versions of the array-based APIs (MakeVarArray, etc).
// - customize the construction, and the OptimizationProblemType enum.
// - support the natural language API.
public partial class Solver {
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
}

}  // namespace Google.OrTools.LinearSolver
