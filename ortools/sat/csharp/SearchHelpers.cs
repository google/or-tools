// Copyright 2010-2025 Google LLC
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

/**
 * <summary>
 * Parent class to create a callback called at each solution.
 * </summary>
 */
public class CpSolverSolutionCallback : SolutionCallback
{
    /**
     * <summary>
     * Returns the value of a linear expression in the current solution.
     * </summary>
     */
    public long Value(LinearExpr e)
    {
        long constant = 0;
        long coefficient = 1;
        LinearExpr expr = e;
        if (terms_ is null)
        {
            terms_ = new Queue<Term>();
        }
        else
        {
            terms_.Clear();
        }

        do
        {
            switch (expr)
            {
            case LinearExprBuilder a:
                constant += coefficient * a.Offset;
                if (coefficient == 1)
                {
                    foreach (Term sub in a.Terms)
                    {
                        terms_.Enqueue(sub);
                    }
                }
                else
                {
                    foreach (Term sub in a.Terms)
                    {
                        terms_.Enqueue(new Term(sub.expr, sub.coefficient * coefficient));
                    }
                }
                break;
            case IntVar intVar:
                int index = intVar.GetIndex();
                long value = SolutionIntegerValue(index);
                constant += coefficient * value;
                break;
            case NotBoolVar:
                throw new ArgumentException("Cannot evaluate a literal in an integer expression.");
            default:
                throw new ArgumentException("Cannot evaluate '" + expr + "' in an integer expression");
            }

            if (!terms_.TryDequeue(out var term))
            {
                break;
            }
            expr = term.expr;
            coefficient = term.coefficient;
        } while (true);

        return constant;
    }

    /**
     * <summary>
     * Returns the value of an integer variable in the current solution.
     * </summary>
     */
    public long Value(IntVar intVar)
    {
        return SolutionIntegerValue(intVar.GetIndex());
    }

    /**
     * <summary>
     * Returns the Boolean value of a literal in the current solution.
     * </summary>
     */
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

    private Queue<Term> terms_;
}

/**
 * <summary>
 * A specialized solution printer.
 * </summary>
 */
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
