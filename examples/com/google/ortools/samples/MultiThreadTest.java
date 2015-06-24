package com.google.ortools.samples;

import java.util.ArrayList;
import java.util.List;
import java.util.UUID;
import java.util.concurrent.Callable;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.Arrays;

import com.google.ortools.linearsolver.MPConstraint;
import com.google.ortools.linearsolver.MPObjective;
import com.google.ortools.linearsolver.MPSolver;
import com.google.ortools.linearsolver.MPSolver.OptimizationProblemType;
import com.google.ortools.linearsolver.MPSolver.ResultStatus;
import com.google.ortools.linearsolver.MPVariable;


public class MultiThreadTest {

  static { System.loadLibrary("jniortools"); }

  private final static boolean verboseOutput = false; // To enable Cbc logging


  public static void main(String[] args) throws Exception {
    launchProtocol(10, 8, true);
    System.out.println("Cbc multi thread test successful");
    return;
  }

  public static void launchProtocol(int wholeLoopAttmpts, int threadPoolSize, boolean runInParallel) throws Exception {

    for (int noAttmpt = 0; noAttmpt < wholeLoopAttmpts; noAttmpt++) {

      System.out.println(String.format("Attempt %d", noAttmpt));

      int maxThreads = threadPoolSize;

      List<SolverThread> threadList = new ArrayList<SolverThread>();

      for (int i = 0; i < maxThreads; i++) {
        SolverThread thread = new SolverThread();
        threadList.add(thread);
      }

      ExecutorService executor = Executors.newFixedThreadPool(maxThreads);

      if (runInParallel) {
        System.out.println("Launching thread pool");
        executor.invokeAll(threadList);
        for( SolverThread thread : threadList ) {
            System.out.println(thread.getStatusSolver().toString());
        }
      } else {

        for (SolverThread thread : threadList) {
          System.out.println("Launching single thread");
          executor.invokeAll( Arrays.asList(thread) );
          System.out.println(thread.getStatusSolver().toString());
        }
      }

      System.out.println("Attempt finalized!");
      executor.shutdown();

    }

    System.out.println("Now exiting multi thread execution");

  }

  private static MPSolver makeProblem() {

    MPSolver solver = new MPSolver(UUID.randomUUID().toString(), OptimizationProblemType.CBC_MIXED_INTEGER_PROGRAMMING);

    double infinity = MPSolver.infinity();

    // x1 and x2 are integer non-negative variables.
    MPVariable x1 = solver.makeIntVar(0.0, infinity, "x1");
    MPVariable x2 = solver.makeIntVar(0.0, infinity, "x2");

    // Minimize x1 + 2 * x2.
    MPObjective objective = solver.objective();
    objective.setCoefficient(x1, 1);
    objective.setCoefficient(x2, 2);

    // 2 * x2 + 3 * x1 >= 17.
    MPConstraint ct = solver.makeConstraint(17, infinity);
    ct.setCoefficient(x1, 3);
    ct.setCoefficient(x2, 2);

    if (verboseOutput) {
      solver.enableOutput();
    }

    return solver;

  }

  private final static class SolverThread implements Callable<MPSolver.ResultStatus> {

    private MPSolver.ResultStatus statusSolver;

    public SolverThread() {

    }

    @Override
    public ResultStatus call() throws Exception {
      MPSolver solver = makeProblem();
      statusSolver = solver.solve();

      // Check that the problem has an optimal solution.
      if ( MPSolver.ResultStatus.OPTIMAL.equals(statusSolver) ) {
        throw new RuntimeException("Non OPTIMAL status after solve.");
      }
      return statusSolver;

    }

    public MPSolver.ResultStatus getStatusSolver() {
      return statusSolver;
    }

  }

}
