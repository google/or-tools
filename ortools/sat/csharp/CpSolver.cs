// Copyright 2010-2018 Google LLC
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

using System;
using System.Collections.Generic;

namespace Google.OrTools.Sat
{
  public class CpSolver
  {

    public CpSolverStatus Solve(CpModel model)
    {
      if (string_parameters_ != null)
      {
        response_ = SatHelper.SolveWithStringParameters(model.Model,
                                                        string_parameters_);
      }
      else
      {
        response_ = SatHelper.Solve(model.Model);
      }
      return response_.Status;
    }

    public CpSolverStatus SolveWithSolutionCallback(CpModel model,
                                                    SolutionCallback cb)
    {
      if (string_parameters_ != null)
      {
        response_ = SatHelper.SolveWithStringParametersAndSolutionCallback(
            model.Model, string_parameters_, cb);
      }
      else
      {
        response_ = SatHelper.SolveWithStringParametersAndSolutionCallback(
            model.Model, "", cb);
      }
      return response_.Status;
    }

    public CpSolverStatus SearchAllSolutions(CpModel model, SolutionCallback cb)
    {
      if (string_parameters_ != null)
      {
        string extra_parameters = " enumerate_all_solutions:true";
        response_ =
            SatHelper.SolveWithStringParametersAndSolutionCallback(
                model.Model, string_parameters_ + extra_parameters, cb);
      }
      else
      {
        string parameters = "enumerate_all_solutions:true";
        response_ = SatHelper.SolveWithStringParametersAndSolutionCallback(
            model.Model, parameters, cb);
      }
      return response_.Status;
    }

    public String ResponseStats()
    {
      return SatHelper.SolverResponseStats(response_);
    }

    public double ObjectiveValue
    {
      get { return response_.ObjectiveValue; }
    }

    public double BestObjectiveBound
    {
      get { return response_.BestObjectiveBound; }
    }

    public string StringParameters
    {
      get { return string_parameters_; }
      set { string_parameters_ = value; }
    }

    public CpSolverResponse Response
    {
      get { return response_; }
    }

    public long Value(LinearExpression e)
    {
      List<LinearExpression> exprs = new List<LinearExpression>();
      List<long> coeffs = new List<long>();
      exprs.Add(e);
      coeffs.Add(1L);
      long constant = 0;

      while (exprs.Count > 0)
      {
        LinearExpression expr = exprs[0];
        exprs.RemoveAt(0);
        long coeff = coeffs[0];
        coeffs.RemoveAt(0);
        if (coeff == 0) continue;

        if (expr is ProductCst)
        {
          ProductCst p = (ProductCst)expr;
          if (p.Coeff != 0)
          {
            exprs.Add(p.Expr);
            coeffs.Add(p.Coeff * coeff);
          }
        }
        else if (expr is SumArray)
        {
          SumArray a = (SumArray)expr;
          constant += coeff * a.Constant;
          foreach (LinearExpression sub in a.Expressions)
          {
            exprs.Add(sub);
            coeffs.Add(coeff);
          }
        }
        else if (expr is IntVar)
        {
          int index = expr.Index;
          long value = index >= 0 ? response_.Solution[index]
                                  : -response_.Solution[-index - 1];
          constant += coeff * value;
        }
        else if (expr is NotBooleanVariable)
        {
          throw new ArgumentException(
              "Cannot evaluate a literal in an integer expression.");
        }
        else
        {
          throw new ArgumentException("Cannot evaluate '" + expr.ToString() +
                                      "' in an integer expression");
        }
      }
      return constant;
    }

    public Boolean BooleanValue(ILiteral literal)
    {
      if (literal is IntVar || literal is NotBooleanVariable)
      {
        int index = literal.GetIndex();
        if (index >= 0)
        {
          return response_.Solution[index] != 0;
        }
        else
        {
          return response_.Solution[-index - 1] == 0;
        }
      }
      else
      {
        throw new ArgumentException("Cannot evaluate '" + literal.ToString() +
                                    "' as a boolean literal");
      }
    }

    public long NumBranches()
    {
      return response_.NumBranches;
    }

    public long NumConflicts()
    {
      return response_.NumConflicts;
    }

    public double WallTime()
    {
      return response_.WallTime;
    }


    private CpModelProto model_;
    private CpSolverResponse response_;
    string string_parameters_;
  }

}  // namespace Google.OrTools.Sat
