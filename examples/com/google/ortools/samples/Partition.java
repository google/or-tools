/**
 *  Copyright (c) 1999-2011, Ecole des Mines de Nantes
 *  All rights reserved.
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are met:
 *
 *      * Redistributions of source code must retain the above copyright
 *        notice, this list of conditions and the following disclaimer.
 *      * Redistributions in binary form must reproduce the above copyright
 *        notice, this list of conditions and the following disclaimer in the
 *        documentation and/or other materials provided with the distribution.
 *      * Neither the name of the Ecole des Mines de Nantes nor the
 *        names of its contributors may be used to endorse or promote products
 *        derived from this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND ANY
 *  EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 *  WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 *  DISCLAIMED. IN NO EVENT SHALL THE REGENTS AND CONTRIBUTORS BE LIABLE FOR ANY
 *  DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 *  (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 *  LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 *  ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *  (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 *  SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
package com.google.ortools.samples;

import com.google.ortools.constraintsolver.*;

/**
 * Partition n numbers into two groups, so that
 * - the sum of the first group equals the sum of the second,
 * - and the sum of the squares of the first group equals the sum of
 *   the squares of the second
 * <br/>
 *
 * @author Charles Prud'homme
 * @since 18/03/11
 */
public class Partition {

  static {
    System.loadLibrary("jniortools");
  }


  /**
   * Partition Problem.
   */
  private static void solve(int m) {
    Solver solver = new Solver("Partition " + m);

    IntVar[] x, y;
    x = solver.makeIntVarArray(m, 1, 2 * m, "x");
    y = solver.makeIntVarArray(m, 1, 2 * m, "y");

    // break symmetries
    for (int i = 0; i < m - 1; i++) {
      solver.addConstraint(solver.makeLess(x[i], x[i + 1]));
      solver.addConstraint(solver.makeLess(y[i], y[i + 1]));
    }
    solver.addConstraint(solver.makeLess(x[0], y[0]));

    IntVar[] xy = new IntVar[2 * m];
    for (int i = m - 1; i >= 0; i--) {
      xy[i] = x[i];
      xy[m + i] = y[i];
    }

    solver.addConstraint(solver.makeAllDifferent(xy));

    int[] coeffs = new int[2 * m];
    for (int i = m - 1; i >= 0; i--) {
      coeffs[i] = 1;
      coeffs[m + i] = -1;
    }
    solver.addConstraint(solver.makeScalProdEquality(xy, coeffs, 0));

    IntVar[] sxy, sx, sy;
    sxy = new IntVar[2 * m];
    sx = new IntVar[m];
    sy = new IntVar[m];
    for (int i = m - 1; i >= 0; i--) {
      sx[i] = solver.makeSquare(x[i]).var();
      sxy[i] = sx[i];
      sy[i] = solver.makeSquare(y[i]).var();
      sxy[m + i] = sy[i];
    }
    solver.addConstraint(solver.makeScalProdEquality(sxy, coeffs, 0));

    solver.addConstraint(
        solver.makeSumEquality(x, 2 * m * (2 * m + 1) / 4));
    solver.addConstraint(
        solver.makeSumEquality(y, 2 * m * (2 * m + 1) / 4));
    solver.addConstraint(
        solver.makeSumEquality(sx, 2 * m * (2 * m + 1) * (4 * m + 1) / 12));
    solver.addConstraint(
        solver.makeSumEquality(sy, 2 * m * (2 * m + 1) * (4 * m + 1) / 12));

    DecisionBuilder db = solver.makeDefaultPhase(xy);
    SolutionCollector collector = solver.makeFirstSolutionCollector();
    collector.add(xy);
    SearchMonitor log = solver.makeSearchLog(10000);
    solver.newSearch(db, log, collector);
    solver.nextSolution();
    System.out.println("Solution solution");
    for (int i = 0; i < m; ++i) {
      System.out.print("[" + collector.value(0, xy[i]) + "] ");
    }
    System.out.printf("\n");
    for (int i = 0; i < m; ++i) {
      System.out.print("[" + collector.value(0, xy[m+i]) + "] ");
    }
    System.out.println();
  }

  public static void main(String[] args) throws Exception {
    Partition.solve(32);
  }


}
