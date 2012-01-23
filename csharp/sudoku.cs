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
using System.Collections;
using System.IO;
using System.Text.RegularExpressions;
using Google.OrTools.ConstraintSolver;


public class Sudoku
{

  /**
   *
   * Solves a Sudoku problem.
   *
   */
  private static void Solve()
  {
    Solver solver = new Solver("Sudoku");

    //
    // data
    //
    int cell_size = 3;
    int n = cell_size * cell_size;

    // 0 marks an unknown value
    int[,] initial_grid = {{0, 6, 0, 0, 5, 0, 0, 2, 0},
                           {0, 0, 0, 3, 0, 0, 0, 9, 0},
                           {7, 0, 0, 6, 0, 0, 0, 1, 0},
                           {0, 0, 6, 0, 3, 0, 4, 0, 0},
                           {0, 0, 4, 0, 7, 0, 1, 0, 0},
                           {0, 0, 5, 0, 9, 0, 8, 0, 0},
                           {0, 4, 0, 0, 0, 1, 0, 0, 6},
                           {0, 3, 0, 0, 0, 8, 0, 0, 0},
                           {0, 2, 0, 0, 4, 0, 0, 5, 0}};
    

    //
    // Decision variables
    //
    IntVar[,] grid =  new IntVar[n,n];
    IntVar[] grid_flat = new IntVar[n * n];
    for(int i = 0; i < n; i++) {
      for(int j = 0; j < n; j++) {
        grid[i,j] = solver.MakeIntVar(1, 9, "grid[" + i + "," + j + "]");
        grid_flat[i * n + j] = grid[i,j];
      }
    }

    //
    // Constraints
    //  

    // init and rows
    for(int i = 0; i < n; i++) {
      IntVar[] row = new IntVar[n];
      for(int j = 0; j < n; j++) {
        if (initial_grid[i,j] > 0) {
          solver.Add(grid[i,j] == initial_grid[i,j]);
        }
        row[j] = grid[i,j];
      }
      solver.Add(row.AllDifferent());
    }

    // columns
    for(int j = 0; j < n; j++) {
      IntVar[] col = new IntVar[n];
      for(int i = 0; i < n; i++) {
        col[i] = grid[i,j];
      }
      solver.Add(col.AllDifferent());
    }

    // cells
    for(int i = 0; i < cell_size; i++) {
      for(int j = 0; j < cell_size; j++) {
        IntVar[] cell = new IntVar[n];
        for(int di = 0; di < cell_size; di++) {
          for(int dj = 0; dj < cell_size; dj++) {
            cell[di * cell_size + dj] = 
              grid[i * cell_size + di,j * cell_size + dj];
          }
        }
        solver.Add(cell.AllDifferent());
      }
    }        


    //
    // Search
    //
    DecisionBuilder db = solver.MakePhase(grid_flat,
                                          Solver.INT_VAR_SIMPLE,
                                          Solver.INT_VALUE_SIMPLE);

    solver.NewSearch(db);

    while (solver.NextSolution()) {
      for(int i = 0; i < n; i++) {
        for(int j = 0; j < n; j++){ 
          Console.Write("{0} ", grid[i,j].Value());
        }
        Console.WriteLine();
      }
      
      Console.WriteLine();
    }

    Console.WriteLine("\nSolutions: {0}", solver.Solutions());
    Console.WriteLine("WallTime: {0}ms", solver.WallTime());
    Console.WriteLine("Failures: {0}", solver.Failures());
    Console.WriteLine("Branches: {0} ", solver.Branches());

    solver.EndSearch();

  }


  public static void Main(String[] args)
  {
    Solve();
  }
}
