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

// The assumptions_sample_sat command is an example of setting assumptions on a model.
package main

import (
	"fmt"

	log "github.com/golang/glog"
	"github.com/google/or-tools/ortools/sat/go/cpmodel"

	cmpb "github.com/google/or-tools/ortools/sat/proto/cpmodel"
)

func assumptionsSampleSat() error {
	model := cpmodel.NewCpModelBuilder()

	domain := cpmodel.NewDomain(0, 10)
	x := model.NewIntVarFromDomain(domain).WithName("x")
	y := model.NewIntVarFromDomain(domain).WithName("y")
	z := model.NewIntVarFromDomain(domain).WithName("z")
	a := model.NewBoolVar().WithName("a")
	b := model.NewBoolVar().WithName("b")
	c := model.NewBoolVar().WithName("c")

	model.AddGreaterThan(x, y).OnlyEnforceIf(a)
	model.AddGreaterThan(y, z).OnlyEnforceIf(b)
	model.AddGreaterThan(z, x).OnlyEnforceIf(c)

	// Add assumptions
	model.AddAssumption(a, b, c)

	m, err := model.Model()
	if err != nil {
		return fmt.Errorf("failed to instantiate the CP model: %w", err)
	}
	response, err := cpmodel.SolveCpModel(m)
	if err != nil {
		return fmt.Errorf("failed to solve the model: %w", err)
	}

	fmt.Printf("status: %s\n", response.GetStatus())
	if response.GetStatus() == cmpb.CpSolverStatus_INFEASIBLE {
		fmt.Println("sufficient assumptions for infeasibility:")
		for _, index := range response.GetSufficientAssumptionsForInfeasibility() {
			fmt.Printf("%d\n", index)
		}
	}

	return nil
}

func main() {
	if err := assumptionsSampleSat(); err != nil {
		log.Exitf("assumptionsSampleSat returned with error: %v", err)
	}
}
