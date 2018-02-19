/*
 * Copyright 2017 Darian Sastre darian.sastre@minimaxlabs.com
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 * 
 *     http://www.apache.org/licenses/LICENSE-2.0
 *     
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 * 
 * ************************************************************************
 * 
 * Each knapsack perceives a different weight for each item. Item values are 
 * the same across knapsacks. Optimizing constrains the count of each item such 
 * that all knapsack capacities are respected, and their values are maximized.
 * 
 * This model was created by Hakan Kjellerstrand (hakank@gmail.com)
 */ 

import com.google.ortools.linearsolver.*;

public class KnapsackMIP {
	static { 
		System.loadLibrary("jniortools"); 
	}
	
	private static MPSolver createSolver (String solverType) {
	    try {
	    		return new MPSolver("MIPDiet",
	                          MPSolver.OptimizationProblemType.valueOf(solverType));
	    } catch (java.lang.IllegalArgumentException e) {
	    		System.err.println("Bad solver type: " + e);
	    		return null;
    		}
  	}

	private static void solve(String solverType) {
		MPSolver solver = createSolver(solverType);
		
		/** variables */ 
		int itemCount = 12;
		int capacityCount = 7;
		
		int[] capacity = {18209, 7692, 1333, 924, 26638, 61188, 13360};
		int[] value = {96, 76, 56, 11, 86, 10, 66, 86, 83, 12, 9, 81};
		int[][] weights = {
				{19, 1, 10, 1, 1, 14, 152, 11, 1, 1, 1, 1},
				{0, 4, 53, 0, 0, 80, 0, 4, 5, 0, 0, 0},
				{4, 660, 3, 0, 30, 0, 3, 0, 4, 90, 0, 0},
				{7, 0, 18, 6, 770, 330, 7, 0, 0, 6, 0, 0},
				{0, 20, 0, 4, 52, 3, 0, 0, 0, 5, 4, 0},
				{0, 0, 40, 70, 4, 63, 0, 0, 60, 0, 4, 0},
				{0, 32, 0, 0, 0, 5, 0, 3, 0, 660, 0, 9}
		};
		
		int maxCapacity = -1;
		for (int c : capacity) {
			if (c > maxCapacity) {
				maxCapacity = c;
			}
		}
		
		MPVariable[] taken = solver.makeIntVarArray(itemCount, 0, maxCapacity);
		
		/** constraints */
		MPConstraint constraints[] = new MPConstraint[capacityCount];
		for (int i = 0; i < capacityCount; i ++) {
			constraints[i] = solver.makeConstraint(0, capacity[i]);
			for (int j = 0; j < itemCount; j ++) {
				constraints[i].setCoefficient(taken[j], weights[i][j]);
			}
		}

		/** objective */
		MPObjective obj = solver.objective();
		obj.setMaximization();
		for (int i = 0; i < itemCount; i ++) {
			obj.setCoefficient(taken[i], value[i]);
		}
		
		solver.solve();
		
		/** printing */
		System.out.println("Max cost: " + obj.value());
		System.out.print("Item quantities: ");
		for (MPVariable var : taken) {
			System.out.print((int) var.solutionValue() + " ");
		}
	}
	
	public static void main(String[] args) {
		solve("CBC_MIXED_INTEGER_PROGRAMMING");
	}
}
