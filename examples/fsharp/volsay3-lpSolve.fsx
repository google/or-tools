(*

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

#I "./lib"
#load "Google.OrTools.FSharp.fsx"

open System
open Google.OrTools.FSharp
open Google.OrTools.LinearSolver

let opts = SolverOpts.Default
            .Name("Volsay3")
            .Goal(Maximize)
            .Objective([30.0;40.0])
            .Matrix([[1.0;3.0;0.0]; [1.0;4.0;1.0]])
            .VectorUpperBound([50.0; 180.0; 40.0])
            .VarLowerBound([0.0; 0.0])
            .VarUpperBound([10000.0; 10000.0])
            .Algorithm(LP CLP)

let slvr = opts |> lpSolve |> SolverSummary
