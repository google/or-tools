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
 * This model was created by Hakan Kjellerstrand (hakank@bonetmail.com)
 *
 * Java version by Darian Sastre (darian.sastre@minimaxlabs.com)
 */ 

import com.google.ortools.linearsolver.MPConstraint;
import com.google.ortools.linearsolver.MPObjective;
import com.google.ortools.linearsolver.MPSolver;
import com.google.ortools.linearsolver.MPVariable;

public class ColoringMIP {
	public static class Edge {
		public int a, b;
		
		public Edge(int a, int b) {
			this.a = a;
			this.b = b;
		}
	}
	
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
  		double infinity = MPSolver.infinity();

  		/** invariants */
		int noCols = 5;			// variables number
		int noNodes = 11;		// constraints number
		
		Edge[] edges = {
			new Edge(1,2),
			new Edge(1,4),
			new Edge(1,7),
			new Edge(1,9),
			new Edge(2,3),
			new Edge(2,6),
			new Edge(2,8),
			new Edge(3,5),
			new Edge(3,7),
			new Edge(3,10),
			new Edge(4,5),
			new Edge(4,6),
			new Edge(4,10),
			new Edge(5,8),
			new Edge(5,9),
			new Edge(6,11),
			new Edge(7,11),
			new Edge(8,11),
			new Edge(9,11),
			new Edge(10,11)
		};
		
		/** variables */
		MPVariable[][] x = new MPVariable[noNodes][noCols]; 
		for (Integer i = 0; i < noNodes; i ++) {
			x[i] = solver.makeBoolVarArray(noCols);
		}
		
		MPVariable[] colUsed = solver.makeBoolVarArray(noCols);
		
		MPObjective obj = solver.objective();
		for (MPVariable objVar : colUsed) {
			obj.setCoefficient(objVar, 1);
		}
		
		/** Bound each vertex to only one color */
		MPConstraint[] constraints = new MPConstraint[noNodes];
		for (int i = 0; i < noNodes; i ++) {
			constraints[i] = solver.makeConstraint(1,1);
			for (int j = 0; j < noCols; j ++) {
				constraints[i].setCoefficient(x[i][j], 1);
			}
		}	
		
		/** Set adjacent nodes to have different colors */
		MPConstraint[][] adjacencies = new MPConstraint[edges.length][noCols];
		for (int i = 0; i < edges.length; i ++) {	
			for (int j = 0; j < noCols; j ++) { 
				adjacencies[i][j] = solver.makeConstraint(-infinity, 0);
				adjacencies[i][j].setCoefficient(x[edges[i].a - 1][j], 1);
				adjacencies[i][j].setCoefficient(x[edges[i].b - 1][j], 1);
				adjacencies[i][j].setCoefficient(colUsed[j], -1);
			}
		}
		
		/** Minimize by default */
		final MPSolver.ResultStatus resultStatus = solver.solve();
		
		/** printing */
		if (resultStatus != MPSolver.ResultStatus.OPTIMAL) {
			System.err.println("The problem does not have an optimal solution!");
			return;
		} else {
			System.out.print("Colors used: ");
			for (MPVariable var : colUsed) {
				System.out.print((int) var.solutionValue() + " ");
			}
			System.out.println("\n");
			
			for (int i = 0; i < noNodes; i ++) {
				System.out.print("Col of vertex " + i + " : ");
				for (int j = 0; j < noCols; j ++) {
					if (x[i][j].solutionValue() > 0) {
						System.out.println(j);
					}
				}
			}
		}
	}

	public static void main(String[] args) {
		System.out.println("---- Integer programming example with SCIP (recommended) ----");
    	solve("SCIP_MIXED_INTEGER_PROGRAMMING");
	    System.out.println("---- Integer programming example with CBC ----");
	    solve("CBC_MIXED_INTEGER_PROGRAMMING");
	    System.out.println("---- Integer programming example with GLPK ----");
	    solve("GLPK_MIXED_INTEGER_PROGRAMMING");
	}
}