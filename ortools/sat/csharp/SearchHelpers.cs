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
        List<Term> terms = new List<Term>();
        terms.Add(new Term(e, 1));
        long constant = 0;

        while (terms.Count > 0)
        {
            Term term = terms[0];
            terms.RemoveAt(0);
            if (term.coefficient == 0)
                continue;

            if (term.expr is LinearExprBuilder)
            {
                LinearExprBuilder a = (LinearExprBuilder)term.expr;
                constant += term.coefficient * a.Offset;
                foreach (Term sub in a.Terms)
                {
                    if (term.coefficient == 1)
                    {
                        terms.Add(sub);
                    }
                    else
                    {
                        terms.Add(new Term(sub.expr, sub.coefficient * term.coefficient));
                    }
                }
            }
            else if (term.expr is IntVar)
            {
                int index = ((IntVar)term.expr).GetIndex();
                long value = SolutionIntegerValue(index);
                constant += term.coefficient * value;
            }
            else if (term.expr is NotBoolVar)
            {
                throw new ArgumentException("Cannot evaluate a literal in an integer expression.");
            }
            else
            {
                throw new ArgumentException("Cannot evaluate '" + term.expr.ToString() + "' in an integer expression");
            }
        }
        return constant;
    }

    public Boolean BooleanValue(ILiteral literal)
    {
        if (literal is BoolVar || literal is NotBoolVar)
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
