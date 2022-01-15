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
using System.Runtime.CompilerServices;

namespace Google.OrTools.Sat
{
    public class CpSolver
    {
        public CpSolverStatus Solve(CpModel model, SolutionCallback cb = null)
        {
            // Setup search.
            CreateSolveWrapper();
            if (string_parameters_ is not null)
            {
                solve_wrapper_.SetStringParameters(string_parameters_);
            }
            if (log_callback_ is not null)
            {
                solve_wrapper_.AddLogCallbackFromClass(log_callback_);
            }
            if (cb is not null)
            {
                solve_wrapper_.AddSolutionCallback(cb);
            }

            response_ = solve_wrapper_.Solve(model.Model);

            // Cleanup search.
            if (cb is not null)
            {
                solve_wrapper_.ClearSolutionCallback(cb);
            }
            ReleaseSolveWrapper();

            return response_.Status;
        }

        [ObsoleteAttribute("This method is obsolete. Call Solve instead.", false)]
        public CpSolverStatus SolveWithSolutionCallback(CpModel model, SolutionCallback cb)
        {
            return Solve(model, cb);
        }

        public CpSolverStatus SearchAllSolutions(CpModel model, SolutionCallback cb)
        {
            string old_parameters = string_parameters_;
            string_parameters_ += " enumerate_all_solutions:true";
            Solve(model, cb);
            string_parameters_ = old_parameters;
            return response_.Status;
        }

        [MethodImpl(MethodImplOptions.Synchronized)]
        public void StopSearch()
        {
            if (solve_wrapper_ is not null)
            {
                solve_wrapper_.StopSearch();
            }
        }

        [MethodImpl(MethodImplOptions.Synchronized)]
        private void CreateSolveWrapper()
        {
            solve_wrapper_ = new SolveWrapper();
        }

        [MethodImpl(MethodImplOptions.Synchronized)]
        private void ReleaseSolveWrapper()
        {
            solve_wrapper_ = null;
        }

        public String ResponseStats()
        {
            return CpSatHelper.SolverResponseStats(response_);
        }

        public double ObjectiveValue
        {
            get {
                return response_.ObjectiveValue;
            }
        }

        public double BestObjectiveBound
        {
            get {
                return response_.BestObjectiveBound;
            }
        }

        public string StringParameters
        {
            get {
                return string_parameters_;
            }
            set {
                string_parameters_ = value;
            }
        }

        public void SetLogCallback(StringToVoidDelegate del)
        {
            log_callback_ = new LogCallbackDelegate(del);
        }

        public CpSolverResponse Response
        {
            get {
                return response_;
            }
        }

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
                {
                    continue;
                }

                if (term.expr is LinearExprBuilder a)
                {
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
                else if (term.expr is IntVar intVar)
                {
                    int index = intVar.GetIndex();
                    long value = index >= 0 ? response_.Solution[index] : -response_.Solution[-index - 1];
                    constant += term.coefficient * value;
                }
                else if (term.expr is NotBoolVar)
                {
                    throw new ArgumentException("Cannot evaluate a literal in an integer expression.");
                }
                else
                {
                    throw new ArgumentException("Cannot evaluate '" + term.expr.ToString() +
                                                "' in an integer expression");
                }
            }
            return constant;
        }

        public Boolean BooleanValue(ILiteral literal)
        {
            if (literal is BoolVar || literal is NotBoolVar)
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
                throw new ArgumentException("Cannot evaluate '" + literal.ToString() + "' as a boolean literal");
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

        public IList<int> SufficientAssumptionsForInfeasibility()
        {
            return response_.SufficientAssumptionsForInfeasibility;
        }

        public String SolutionInfo()
        {
            return response_.SolutionInfo;
        }

        private CpSolverResponse response_;
        private LogCallback log_callback_;
        private string string_parameters_;
        private SolveWrapper solve_wrapper_;
    }

    class LogCallbackDelegate : LogCallback
    {
        public LogCallbackDelegate(StringToVoidDelegate del)
        {
            this.delegate_ = del;
        }

        public override void NewMessage(string message)
        {
            delegate_(message);
        }

        private StringToVoidDelegate delegate_;
    }

} // namespace Google.OrTools.Sat
