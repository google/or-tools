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
// The solution_hinting_sample_sat command is an example of setting solution hints on the model.
package main

import (
	"fmt"

	log "github.com/golang/glog"
	"github.com/google/or-tools/ortools/sat/go/cpmodel"

	cmpb "github.com/google/or-tools/ortools/sat/proto/cpmodel"
)

func solutionHintingSampleSat() error {
	model := cpmodel.NewCpModelBuilder()

	domain := cpmodel.NewDomain(0, 2)
	x := model.NewIntVarFromDomain(domain).WithName("x")
	y := model.NewIntVarFromDomain(domain).WithName("y")
	z := model.NewIntVarFromDomain(domain).WithName("z")

	model.AddNotEqual(x, y)

	model.Maximize(cpmodel.NewLinearExpr().AddWeightedSum([]cpmodel.LinearArgument{x, y, z}, []int64{1, 2, 3}))

	// Solution hinting: x <- 1, y <- 2
	hint := &cpmodel.Hint{Ints: map[cpmodel.IntVar]int64{x: 7}}
	model.SetHint(hint)

	m, err := model.Model()
	if err != nil {
		return fmt.Errorf("failed to instantiate the CP model: %w", err)
	}
	response, err := cpmodel.SolveCpModel(m)
	if err != nil {
		return fmt.Errorf("failed to solve the model: %w", err)
	}

	fmt.Printf("Status: %v\n", response.GetStatus())

	if response.GetStatus() == cmpb.CpSolverStatus_OPTIMAL {
		fmt.Printf(" x = %v\n", cpmodel.SolutionIntegerValue(response, x))
		fmt.Printf(" y = %v\n", cpmodel.SolutionIntegerValue(response, y))
		fmt.Printf(" z = %v\n", cpmodel.SolutionIntegerValue(response, z))
	}

	return nil
}

func main() {
	if err := solutionHintingSampleSat(); err != nil {
		log.Exitf("solutionHintingSampleSat returned with error: %v", err)
	}
}

// [END program]
