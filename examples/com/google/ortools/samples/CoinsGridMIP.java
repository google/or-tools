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
 *  Coins grid problem in Google CP Solver.
 * 
 *  Problem from
 *  Tony Hurlimann: "A coin puzzle - SVOR-contest 2007"
 *  http://www.svor.ch/competitions/competition2007/AsroContestSolution.pdf
 *  "
 *  In a quadratic grid (or a larger chessboard) with 31x31 cells, one should
 *  place coins in such a way that the following conditions are fulfilled:
 * 	  1. In each row exactly 14 coins must be placed.
 * 	  2. In each column exactly 14 coins must be placed.
 *    3. The sum of the quadratic horizontal distance from the main diagonal
 *    	 of all cells containing a coin must be as small as possible.
 *    4. In each cell at most one coin can be placed.
 *  The description says to place 14x31 = 434 coins on the chessboard each row
 *  containing 14 coins and each column also containing 14 coins.
 *  "
 * 
 *  This is a Java MIP version of
 *  	  http://www.hakank.org/google_or_tools/coins_grid_mip.py
 *  
 *  	  which is the MIP version of
 *  		http://www.hakank.org/google_or_tools/coins_grid.py
 *  
 *  	  by Hakan Kjellerstrand (hakank@bonetmail.com).
 *  
 *  Java version by Darian Sastre (darian.sastre@minimaxlabs.com)
 */

import com.google.ortools.linearsolver.*;

public class CoinsGridMIP {
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
		
		/** invariants */
		int n = 31;
		int c = 14;
		
		/** variables */
		MPVariable[][] x = new MPVariable[n][n];
		for (int i = 0; i < n; i ++) {
			x[i] = solver.makeBoolVarArray(n);
		}
		
		/** constraints & objective */
		MPConstraint[] constraints = new MPConstraint[2 * n];
		MPObjective obj = solver.objective();
		
		for (int i = 0; i < n; i ++) {
			constraints[2*i] = solver.makeConstraint(c, c);
			constraints[2*i + 1] = solver.makeConstraint(c, c);
			
			for (int j = 0; j < n; j ++) {
				constraints[2*i].setCoefficient(x[i][j], 1);
				constraints[2*i + 1].setCoefficient(x[j][i], 1);
				
				obj.setCoefficient(x[i][j], (i-j) * (j-i));
			}
		}
		
		solver.solve();
		
		for (int i = 0; i < n; i ++) {
			for (int j = 0; j < n; j ++) {
				System.out.print((int) x[i][j].solutionValue() + " ");
			}
			System.out.println();
		}
	}
	
	public static void main(String[] args) {
		solve("CBC_MIXED_INTEGER_PROGRAMMING");
	}
}
