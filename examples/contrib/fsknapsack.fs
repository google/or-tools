(*
Copyright 2010-2017 Google
Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
*)

open Google.OrTools.FSharp

let profits = [ 360L; 83L; 59L; 130L; 431L; 67L; 230L; 52L; 93L; 125L; 670L; 892L; 600L; 38L; 48L; 147L; 78L; 256L; 63L; 17L; 120L; 164L; 432L; 35L; 92L; 110L; 22L; 42L; 50L; 323L; 514L; 28L; 87L; 73L; 78L; 15L; 26L; 78L; 210L; 36L; 85L; 189L; 274L; 43L; 33L; 10L; 19L; 389L; 276L; 312L ]
let weights = array2D [ [ 7L; 0L; 30L; 22L; 80L; 94L; 11L; 81L; 70L; 64L; 59L; 18L; 0L;
36L; 3L; 8L; 15L; 42L; 9L; 0L; 42L; 47L; 52L; 32L; 26L; 48L; 55L; 6L; 29L; 84L;
2L; 4L; 18L; 56L; 7L; 29L; 93L; 44L; 71L; 3L; 86L; 66L; 31L; 65L; 0L; 79L; 20L;
65L; 52L; 13L ] ]
let capacities = [ 850L ]

printfn "Solving knapsack with %i items and %i dimensions" (profits.Length) (weights.Length)

let ks = knapsackSolve "test" KnapsackSolverAlgorithm.MultidimensionBranchAndBound profits weights capacities
let computedProfit = ks.Solve();
let expectedProfit = 7534L
printfn "Optimal Profit = %d \nexpected = %d \nOptimal Soltion = %b " computedProfit expectedProfit (ks.IsSolutionOptimal())
