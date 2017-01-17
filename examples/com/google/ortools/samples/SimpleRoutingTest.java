package com.google.ortools.samples;

import java.util.ArrayList;

import com.google.ortools.constraintsolver.Assignment;
import com.google.ortools.constraintsolver.NodeEvaluator2;
import com.google.ortools.constraintsolver.RoutingModel;
import com.google.ortools.constraintsolver.FirstSolutionStrategy;
import com.google.ortools.constraintsolver.RoutingSearchParameters;

public class SimpleRoutingTest {

  //Static Add Library
  static { System.loadLibrary("jniortools"); }

  private ArrayList<Integer> globalRes;
  private long globalResCost;
  private int[][] costMatrix;

  public ArrayList<Integer> getGlobalRes() {return globalRes;}
  public void setGlobalRes(ArrayList<Integer> globalRes) {this.globalRes = globalRes;}
  public long getGlobalResCost() {return globalResCost;}
  public void setGlobalResCost(int globalResCost) {this.globalResCost = globalResCost;}
  public int[][] getCostMatrix() {return costMatrix;}
  public void setCostMatrix(int[][] costMatrix) {this.costMatrix = costMatrix;}

  public SimpleRoutingTest(int[][] costMatrix) {
    super();
    this.costMatrix = costMatrix;
    globalRes = new ArrayList();
  }

  //Node Distance Evaluation
  public static class NodeDistance extends NodeEvaluator2 {
    private int[][] costMatrix;

    public NodeDistance(int[][] costMatrix) {
      this.costMatrix = costMatrix;
    }

    @Override
    public long run(int firstIndex, int secondIndex) {
      return costMatrix[firstIndex][secondIndex];
    }
  }

  //Solve Method
  public void solve() {
    RoutingModel routing = new RoutingModel(costMatrix.length, 1, 0);
    RoutingSearchParameters parameters =
        RoutingSearchParameters.newBuilder()
            .mergeFrom(RoutingModel.defaultSearchParameters())
            .setFirstSolutionStrategy(FirstSolutionStrategy.Value.PATH_CHEAPEST_ARC)
            .build();
    NodeDistance distances = new NodeDistance(costMatrix);
    routing.setArcCostEvaluatorOfAllVehicles(distances);

    Assignment solution = routing.solve();
    if (solution != null) {
      int route_number = 0;
      for (long node = routing.start(route_number); !routing.isEnd(node); node = solution.value(routing.nextVar(node))) {
        globalRes.add((int) node);
      }
    }
    globalResCost = solution.objectiveValue();
    System.out.println("cost = " + globalResCost);
  }


  public static void main(String[] args) throws Exception {
    int[][] values = new int[4][4];
    values[0][0]=0;
    values[0][1]=5;
    values[0][2]=3;
    values[0][3]=6;
    values[1][0]=5;
    values[1][1]=0;
    values[1][2]=8;
    values[1][3]=1;
    values[2][0]=3;
    values[2][1]=8;
    values[2][2]=0;
    values[2][3]=4;
    values[3][0]=6;
    values[3][1]=1;
    values[3][2]=4;
    values[3][3]=0;
    SimpleRoutingTest model = new SimpleRoutingTest(values);
    model.solve();
  }
}
