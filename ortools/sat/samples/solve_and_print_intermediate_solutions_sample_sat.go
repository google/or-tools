// Copyright 2010-2024 Google LLC
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

// The solve_and_print_intermediate_solutions_sample_sat command
package main

import (
	"fmt"

	log "github.com/golang/glog"
	"github.com/google/or-tools/ortools/sat/go/cpmodel"
	sppb "github.com/google/or-tools/ortools/sat/proto/satparameters"
	"google.golang.org/protobuf/proto"
)

func solveAndPrintIntermediateSolutionsSampleSat() error {
	model := cpmodel.NewCpModelBuilder()

	domain := cpmodel.NewDomain(0, 2)
	x := model.NewIntVarFromDomain(domain).WithName("x")
	y := model.NewIntVarFromDomain(domain).WithName("y")
	z := model.NewIntVarFromDomain(domain).WithName("z")

	model.AddNotEqual(x, y)

	model.Maximize(cpmodel.NewLinearExpr().Add(x).AddTerm(y, 2).AddTerm(z, 3))

	// Create a solver and solve with a fixed search.
	m, err := model.Model()
	if err != nil {
		return fmt.Errorf("failed to instantiate the CP model: %w", err)
	}

	// Currently, the CpModelBuilder does not allow for callbacks, so intermediate solutions
	// cannot be printed while solving. However, the CP-SAT solver does allow for returning
	// the intermediate solutions found while solving in the response.
	params := &sppb.SatParameters{
		FillAdditionalSolutionsInResponse: proto.Bool(true),
		SolutionPoolSize:                  proto.Int32(10),
	}
	response, err := cpmodel.SolveCpModelWithParameters(m, params)
	if err != nil {
		return fmt.Errorf("failed to solve the model: %w", err)
	}

	fmt.Println("Number of intermediate solutions found: ", len(response.GetAdditionalSolutions()))

	fmt.Println("Optimal solution:")
	fmt.Printf("  x = %v\n", cpmodel.SolutionIntegerValue(response, x))
	fmt.Printf("  y = %v\n", cpmodel.SolutionIntegerValue(response, y))
	fmt.Printf("  z = %v\n", cpmodel.SolutionIntegerValue(response, z))

	return nil
}

func main() {
	if err := solveAndPrintIntermediateSolutionsSampleSat(); err != nil {
		log.Exitf("solveAndPrintIntermediateSolutionsSampleSat returned with error: %v", err)
	}
}
