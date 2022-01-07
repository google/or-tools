// Copyright 2010-2021 Google LLC
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

public class CpSolverSolutionCallback : SolutionCallback
{
    public long Value(LinearExpr e)
    {
        List<LinearExpr> exprs = new List<LinearExpr>();
        List<long> coeffs = new List<long>();
        exprs.Add(e);
        coeffs.Add(1L);
        long constant = 0;

        while (exprs.Count > 0)
        {
            LinearExpr expr = exprs[0];
            exprs.RemoveAt(0);
            long coeff = coeffs[0];
            coeffs.RemoveAt(0);
            if (coeff == 0)
                continue;

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
                constant += coeff * a.Offset;
                foreach (LinearExpr sub in a.Expressions)
                {
                    exprs.Add(sub);
                    coeffs.Add(coeff);
                }
            }
            else if (expr is IntVar)
            {
                int index = expr.Index;
                long value = SolutionIntegerValue(index);
                constant += coeff * value;
            }
            else if (expr is NotBooleanVariable)
            {
                throw new ArgumentException("Cannot evaluate a literal in an integer expression.");
            }
            else
            {
                throw new ArgumentException("Cannot evaluate '" + expr.ToString() + "' in an integer expression");
            }
        }
        return constant;
    }

    public Boolean BooleanValue(ILiteral literal)
    {
        if (literal is IntVar || literal is NotBooleanVariable)
        {
            int index = literal.GetIndex();
            return SolutionBooleanValue(index);
        }
        else
        {
            throw new ArgumentException("Cannot evaluate '" + literal.ToString() + "' as a boolean literal");
        }
    }
}

public class ObjectiveSolutionPrinter : CpSolverSolutionCallback
{
    private DateTime _startTime;
    private int _solutionCount;

    public ObjectiveSolutionPrinter()
    {
        _startTime = DateTime.Now;
    }

    public override void OnSolutionCallback()
    {
        var currentTime = DateTime.Now;
        var objective = ObjectiveValue();
        var objectiveBound = BestObjectiveBound();
        var objLb = Math.Min(objective, objectiveBound);
        var objUb = Math.Max(objective, objectiveBound);
        var time = currentTime - _startTime;

        Console.WriteLine(
            value: $"Solution {_solutionCount}, time = {time.TotalSeconds} s, objective = [{objLb}, {objUb}]");

        _solutionCount++;
    }

    public int solutionCount() => _solutionCount;
}

} // namespace Google.OrTools.Sat
