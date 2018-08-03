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

import com.google.ortools.sat.*;

import java.util.ArrayList;
import java.util.List;

public class RankingSample {

  static {
    System.loadLibrary("jniortools");
  }

  static void RankTasks(CpModel model,
                        IntVar[] starts,
                        ILiteral[] presences,
                        IntVar[] ranks) {
    int num_tasks = starts.length;

    // Creates precedence variables between pairs of intervals.
    ILiteral[][] precedences = new ILiteral[num_tasks][num_tasks];
    for (int i = 0; i < num_tasks; ++i) {
      for (int j = 0; j < num_tasks; ++j) {
        if (i == j) {
          precedences[i][i] = presences[i];
        } else {
          IntVar prec = model.newBoolVar(String.format("%d before %d", i, j));
          precedences[i][j] = prec;
          model.addLessOrEqualWithOffset(starts[i], starts[j], 1).onlyEnforceIf(
              prec);
        }
      }
    }

    // Treats optional intervals.
    for (int i = 0; i < num_tasks - 1; ++i) {
      for (int j = i + 1; j < num_tasks; ++j) {
        List<ILiteral> list = new ArrayList<ILiteral>();
        list.add(precedences[i][j]);
        list.add(precedences[j][i]);
        list.add(presences[i].not());
        // Makes sure that if i is not performed, all precedences are false.
        model.addImplication(presences[i].not(), precedences[i][j].not());
        model.addImplication(presences[i].not(), precedences[j][i].not());
        list.add(presences[j].not());
        // Makes sure that if j is not performed, all precedences are false.
        model.addImplication(presences[j].not(), precedences[i][j].not());
        model.addImplication(presences[j].not(), precedences[j][i].not());
        // The following bool_or will enforce that for any two intervals:
        //    i precedes j or j precedes i or at least one interval is not
        //        performed.
        model.addBoolOr(list.toArray(new ILiteral[list.size()]));
        // Redundant constraint: it propagates early that at most one precedence
        // is true.
        model.addImplication(precedences[i][j], precedences[j][i].not());
        model.addImplication(precedences[j][i], precedences[i][j].not());
      }
    }

    // Links precedences and ranks.
    for (int i = 0; i < num_tasks; ++i) {
      IntVar[] vars = new IntVar[num_tasks + 1];
      int[] coefs = new int[num_tasks + 1];
      for (int j = 0; j < num_tasks; ++j) {
        vars[j] = (IntVar)precedences[j][i];
        coefs[j] = 1;
      }
      vars[num_tasks] = ranks[i];
      coefs[num_tasks] = -1;
      // ranks == sum(precedences) - 1;
      model.addScalProdEqual(vars, coefs, 1);
    }
  }

  static void RankingSample() {
    CpModel model = new CpModel();
    // Three weeks.
    int horizon = 100;
    int num_tasks = 4;

    IntVar[] starts = new IntVar[num_tasks];
    IntVar[] ends = new IntVar[num_tasks];
    IntervalVar[] intervals = new IntervalVar[num_tasks];
    ILiteral[] presences = new ILiteral[num_tasks];
    IntVar[] ranks = new IntVar[num_tasks];

    IntVar true_var = model.newConstant(1);

    // Creates intervals, half of them are optional.
    for (int t = 0; t < num_tasks; ++t) {
      starts[t] = model.newIntVar(0, horizon, "start_" + t);
      int duration = t + 1;
      ends[t] = model.newIntVar(0, horizon, "end_" + t);
      if (t < num_tasks / 2) {
        intervals[t] = model.newIntervalVar(starts[t], duration, ends[t],
                                            "interval_" + t);
        presences[t] = true_var;
      } else {
        presences[t] = model.newBoolVar("presence_" + t);
        intervals[t] = model.newOptionalIntervalVar(
            starts[t], duration, ends[t], presences[t],
            "o_interval_" + t);
      }

      // Ranks = -1 if and only if the tasks is not performed.
      ranks[t] = model.newIntVar(-1, num_tasks - 1, "rank_" + t);
    }

    // Adds NoOverlap constraint.
    model.addNoOverlap(intervals);

    // Adds ranking constraint.
    RankTasks(model, starts, presences, ranks);

    // Adds a constraint on ranks (ranks[0] < ranks[1]).
    model.addLessOrEqualWithOffset(ranks[0], ranks[1], 1);

    // Creates makespan variable.
    IntVar makespan = model.newIntVar(0, horizon, "makespan");
    for (int t = 0; t < num_tasks; ++t) {
      model.addLessOrEqual(ends[t], makespan).onlyEnforceIf(presences[t]);
    }
    // Minimizes makespan - fixed gain per tasks performed.
    // As the fixed cost is less that the duration of the last interval,
    // the solver will not perform the last interval.
    IntVar[] objective_vars = new IntVar[num_tasks + 1];
    int[] objective_coefs = new int[num_tasks + 1];
    for (int t = 0; t < num_tasks; ++t) {
      objective_vars[t] = (IntVar)presences[t];
      objective_coefs[t] = -7;
    }
    objective_vars[num_tasks] = makespan;
    objective_coefs[num_tasks] = 2;
    model.minimizeScalProd(objective_vars, objective_coefs);

    // Creates a solver and solves the model.
    CpSolver solver = new CpSolver();
    CpSolverStatus status = solver.solve(model);

    if (status == CpSolverStatus.OPTIMAL) {
      System.out.println("Optimal cost: " +  solver.objectiveValue());
      System.out.println("Makespan: " + solver.value(makespan));
      for (int t = 0; t < num_tasks; ++t) {
        if (solver.booleanValue(presences[t])) {
          System.out.println(String.format(
              "Task %d starts at %d with rank %d",
              t, solver.value(starts[t]), solver.value(ranks[t])));
        } else {
          System.out.println(String.format(
              "Task %d in not performed and ranked at %d", t,
              solver.value(ranks[t])));
        }
      }
    } else {
      System.out.println("Solver exited with nonoptimal status: " + status);
    }
  }

  public static void main(String[] args) throws Exception {
    RankingSample();
  }
}
