// Copyright 2011 Hakan Kjellerstrand hakank@bonetmail.com
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

package com.google.ortools.constraintsolver.samples;

import java.io.*;
import java.util.*;
import java.text.*;

import com.google.ortools.constraintsolver.DecisionBuilder;
import com.google.ortools.constraintsolver.IntVar;
import com.google.ortools.constraintsolver.Solver;

public class MagicSquare {

    static {
        System.loadLibrary("jniconstraintsolver");
    }


    /**
     *
     * Solves the Magic Square problem.
     * See http://www.hakank.org/google_or_tools/magic_square.py
     *
     */
    private static void solve(int n, int num) {

        Solver solver = new Solver("MagicSquare");

        System.out.println("n: " + n);

        //
        // variables
        //
        IntVar x[][] = new IntVar[n][n];

        // for the branching
        IntVar x_flat[] = new IntVar[n*n];

        // 
        // constraints
        // 
        long s = (n*(n*n+1))/2;
        System.out.println("s: " + s);
        // IntVar s = solver.makeIntVar(0, n*n*n, "s");

        ArrayList<IntVar> diag1 = new ArrayList<IntVar>();    
        ArrayList<IntVar> diag2 = new ArrayList<IntVar>();    
        for(int i = 0; i < n; i++) {
            ArrayList<IntVar> row = new ArrayList<IntVar>();
            for(int j = 0; j < n; j++) {
                x[i][j] = solver.makeIntVar(1, n*n, "x["+i+","+j+"]");
                x_flat[i*n+j] = x[i][j];

                row.add(x[i][j]);
            }
            // sum row to s
            solver.addConstraint(
                solver.makeSumEquality(row.toArray(new IntVar[1]), s));

            diag1.add(x[i][i]);
            diag2.add(x[i][n-i-1]);

        }
        // sum diagonals to s
        solver.addConstraint(
            solver.makeSumEquality(diag1.toArray(new IntVar[1]), s));
        solver.addConstraint(
            solver.makeSumEquality(diag2.toArray(new IntVar[1]), s));

        // sum columns to s
        for(int j = 0; j < n; j++) {
            ArrayList<IntVar> col = new ArrayList<IntVar>();
            for(int i = 0; i < n; i++) {
                col.add(x[i][j]);
            }
            solver.addConstraint(
                solver.makeSumEquality(col.toArray(new IntVar[1]), s));
        }

        // all are different
        solver.addConstraint(solver.makeAllDifferent(x_flat, true));

        // symmetry breaking: upper left is 1
        // solver.addConstraint(solver.makeEquality(x[0][0], 1));


        //
        // Solve
        //
        DecisionBuilder db = solver.makePhase(x_flat,
                                              solver.CHOOSE_FIRST_UNBOUND,
                                              solver.ASSIGN_CENTER_VALUE);
        solver.newSearch(db);
        int c = 0;
        while (solver.nextSolution()) {
            for(int i = 0; i < n; i++) {
                for(int j = 0; j < n; j++) {
                    System.out.print(x[i][j].value() + " ");
                }
                System.out.println();
            }
            System.out.println();
            c++;
            if (num > 0 && c >= num) {
                break;
            }
        }
        solver.endSearch();

        // Statistics
        System.out.println();
        System.out.println("Solutions: " + solver.solutions());
        System.out.println("Failures: " + solver.failures());
        System.out.println("Branches: " + solver.branches());
        System.out.println("Wall time: " + solver.wall_time() + "ms");

    }

    public static void main(String[] args) throws Exception {
        int n = 4;
        int num = 0;

        if (args.length > 0) {
            n = Integer.parseInt(args[0]);
        }

        if (args.length > 1) {
            num = Integer.parseInt(args[1]);
        }

        MagicSquare.solve(n, num);
    }
}
