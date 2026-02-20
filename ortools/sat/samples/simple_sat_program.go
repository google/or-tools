// Copyright 2010-2025 Google LLC
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

// [START program]
// The simple_sat_program command is an example of a simple sat program.
package main

import (
	"fmt"

	log "github.com/golang/glog"
	"github.com/google/or-tools/ortools/sat/go/cpmodel"

	cmpb "github.com/google/or-tools/ortools/sat/proto/cpmodel"
)

func simpleSatProgram() error {
	model := cpmodel.NewCpModelBuilder()

	domain := cpmodel.NewDomain(0, 2)
	x := model.NewIntVarFromDomain(domain).WithName("x")
	y := model.NewIntVarFromDomain(domain).WithName("y")
	z := model.NewIntVarFromDomain(domain).WithName("z")

	model.AddNotEqual(x, y)

	m, err := model.Model()
	if err != nil {
		return fmt.Errorf("failed to instantiate the CP model: %w", err)
	}
	response, err := cpmodel.SolveCpModel(m)
	if err != nil {
		return fmt.Errorf("failed to solve the model: %w", err)
	}

	switch response.GetStatus() {
	case cmpb.CpSolverStatus_OPTIMAL, cmpb.CpSolverStatus_FEASIBLE:
		fmt.Printf("x = %d\n", cpmodel.SolutionIntegerValue(response, x))
		fmt.Printf("y = %d\n", cpmodel.SolutionIntegerValue(response, y))
		fmt.Printf("z = %d\n", cpmodel.SolutionIntegerValue(response, z))
	default:
		fmt.Println("No solution found.")
	}

	return nil
}

func main() {
	if err := simpleSatProgram(); err != nil {
		log.Exitf("simpleSatProgram returned with error: %v", err)
	}
}

// [END program]
