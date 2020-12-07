//
// Copyright 2012 Hakan Kjellerstrand
//
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
using Google.OrTools.ConstraintSolver;

public class MrSmith
{
    /**
     *
     * Mr Smith problem.
     *
     * From an IF Prolog example (http://www.ifcomputer.de/)
     * """
     * The Smith family and their three children want to pay a visit but they
     * do not all have the time to do so. Following are few hints who will go
     * and who will not:
     *  o If Mr Smith comes, his wife will come too.
     *  o At least one of their two sons Matt and John will come.
     *  o Either Mrs Smith or Tim will come, but not both.
     *  o Either Tim and John will come, or neither will come.
     *  o If Matt comes, then John and his father will
     *    also come.
     * """
     *
     * The answer should be:
     * Mr_Smith_comes      =  0
     * Mrs_Smith_comes     =  0
     * Matt_comes          =  0
     * John_comes          =  1
     * Tim_comes           =  1
     *
     *
     * Also see http://www.hakank.org/or-tools/mr_smith.py
     *
     */
    private static void Solve()
    {
        Solver solver = new Solver("MrSmith");

        //
        // Data
        //
        int n = 5;

        //
        // Decision variables
        //
        IntVar[] x = solver.MakeIntVarArray(n, 0, 1, "x");
        IntVar Mr_Smith = x[0];
        IntVar Mrs_Smith = x[1];
        IntVar Matt = x[2];
        IntVar John = x[3];
        IntVar Tim = x[4];

        //
        // Constraints
        //

        //
        // I've kept the MiniZinc constraints for clarity
        // and debugging.
        //

        // If Mr Smith comes then his wife will come too.
        // (Mr_Smith -> Mrs_Smith)
        solver.Add(Mr_Smith - Mrs_Smith <= 0);

        // At least one of their two sons Matt and John will come.
        // (Matt \/ John)
        solver.Add(Matt + John >= 1);

        // Either Mrs Smith or Tim will come but not both.
        // bool2int(Mrs_Smith) + bool2int(Tim) = 1
        // (Mrs_Smith xor Tim)
        solver.Add(Mrs_Smith + Tim == 1);

        // Either Tim and John will come or neither will come.
        // (Tim = John)
        solver.Add(Tim == John);

        // If Matt comes /\ then John and his father will also come.
        // (Matt -> (John /\ Mr_Smith))
        solver.Add(Matt - (John * Mr_Smith) <= 0);

        //
        // Search
        //
        DecisionBuilder db = solver.MakePhase(x, Solver.CHOOSE_MIN_SIZE_LOWEST_MIN, Solver.ASSIGN_CENTER_VALUE);

        solver.NewSearch(db);

        while (solver.NextSolution())
        {
            for (int i = 0; i < n; i++)
            {
                Console.Write(x[i].Value() + " ");
            }
            Console.WriteLine("\n");
            Console.WriteLine("Mr Smith : {0}", Mr_Smith.Value());
            Console.WriteLine("Mrs Smith: {0}", Mrs_Smith.Value());
            Console.WriteLine("Matt     : {0}", Matt.Value());
            Console.WriteLine("John     : {0}", John.Value());
            Console.WriteLine("Tim      : {0}", Tim.Value());
        }

        Console.WriteLine("\nSolutions: " + solver.Solutions());
        Console.WriteLine("WallTime: " + solver.WallTime() + "ms ");
        Console.WriteLine("Failures: " + solver.Failures());
        Console.WriteLine("Branches: " + solver.Branches());

        solver.EndSearch();
    }

    public static void Main(String[] args)
    {
        Solve();
    }
}
