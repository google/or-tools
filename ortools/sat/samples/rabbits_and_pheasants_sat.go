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

// The rabbits_and_pheasants_sat command is an example of a simple sat program that
// solves the rabbits and pheasants problem.
package main

import (
	"fmt"

	log "github.com/golang/glog"
	"github.com/google/or-tools/ortools/sat/go/cpmodel"
	cmpb "github.com/google/or-tools/ortools/sat/proto/cpmodel"
)

const numAnimals = 20

func rabbitsAndPheasants() error {
	model := cpmodel.NewCpModelBuilder()

	allAnimals := cpmodel.NewDomain(0, numAnimals)
	rabbits := model.NewIntVarFromDomain(allAnimals).WithName("rabbits")
	pheasants := model.NewIntVarFromDomain(allAnimals).WithName("pheasants")

	model.AddEquality(cpmodel.NewLinearExpr().AddSum(rabbits, pheasants), cpmodel.NewConstant(numAnimals))
	model.AddEquality(cpmodel.NewLinearExpr().AddTerm(rabbits, 4).AddTerm(pheasants, 2), cpmodel.NewConstant(56))

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
		fmt.Printf("There are %d rabbits and %d pheasants.\n",
			cpmodel.SolutionIntegerValue(response, rabbits),
			cpmodel.SolutionIntegerValue(response, pheasants))
	default:
		fmt.Println("No solution found.")
	}

	return nil
}

func main() {
	if err := rabbitsAndPheasants(); err != nil {
		log.Exitf("rabbitsAndPheasants returned with error: %v", err)
	}
}
