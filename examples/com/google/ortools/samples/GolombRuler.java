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

import com.google.ortools.constraintsolver.DecisionBuilder;
import com.google.ortools.constraintsolver.IntExpr;
import com.google.ortools.constraintsolver.IntVar;
import com.google.ortools.constraintsolver.OptimizeVar;
import com.google.ortools.constraintsolver.SearchMonitor;
import com.google.ortools.constraintsolver.SolutionCollector;
import com.google.ortools.constraintsolver.Solver;

/**
 * Golomb ruler problem
 * <br/>
 *
 * @author Charles Prud'homme
 * @since 17/03/11
 */
public class GolombRuler {
  static {
    System.loadLibrary("jniortools");
  }


  /**
   * Golomb Ruler Problem.
   */
  private static void solve(int m) {
    Solver solver = new Solver("GR " + m);

    IntVar[] ticks =
        solver.makeIntVarArray(m,
                               0,
                               ((m < 31) ? (1 << (m + 1)) - 1 : 9999),
                               "ticks");

    solver.addConstraint(solver.makeEquality(ticks[0], 0));

    for (int i = 0; i < ticks.length - 1; i++) {
      solver.addConstraint(solver.makeLess(ticks[i], ticks[i + 1]));
    }

    IntVar[] diff = new IntVar[(m * m - m) / 2];

    for (int k = 0, i = 0; i < m - 1; i++) {
      for (int j = i + 1; j < m; j++, k++) {
        diff[k] = solver.makeDifference(ticks[j], ticks[i]).var();
        solver.addConstraint(
            solver.makeGreaterOrEqual(diff[k], (j - i) * (j - i + 1) / 2));
      }
    }

    solver.addConstraint(solver.makeAllDifferent(diff));

    // break symetries
    if (m > 2) {
      solver.addConstraint(solver.makeLess(diff[0], diff[diff.length - 1]));
    }

    OptimizeVar opt =  solver.makeMinimize(ticks[m - 1], 1);
    DecisionBuilder db = solver.makePhase(ticks,
                                          solver.CHOOSE_MIN_SIZE_LOWEST_MIN,
                                          solver.ASSIGN_MIN_VALUE);
    SolutionCollector collector = solver.makeLastSolutionCollector();
    collector.add(ticks);
    collector.addObjective(ticks[m - 1]);
    SearchMonitor log = solver.makeSearchLog(10000, opt);
    solver.solve(db, opt, log, collector);
    System.out.println("Optimal solution = " + collector.objectiveValue(0));
    for (int i = 0; i < m; ++i) {
      System.out.print("[" + collector.value(0, ticks[i]) + "] ");
    }
    System.out.println();
  }

  public static void main(String[] args) throws Exception {
    GolombRuler.solve(8);
  }
}
