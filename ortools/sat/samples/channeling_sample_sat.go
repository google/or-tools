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

// The channeling_sample_sat command is a simple example of a channeling constraint.
package main

import (
	"fmt"

	log "github.com/golang/glog"
	"github.com/google/or-tools/ortools/sat/go/cpmodel"
	cmpb "github.com/google/or-tools/ortools/sat/proto/cpmodel"
	sppb "github.com/google/or-tools/ortools/sat/proto/satparameters"
	"google.golang.org/protobuf/proto"
)

func channelingSampleSat() error {
	// Create the CP-SAT model.
	model := cpmodel.NewCpModelBuilder()

	// Declare our two primary variables.
	x := model.NewIntVar(0, 10)
	y := model.NewIntVar(0, 10)

	// Declare our intermediate Boolean variable.
	b := model.NewBoolVar()

	// Implement b == (x > 5).
	model.AddGreaterOrEqual(x, cpmodel.NewConstant(5)).OnlyEnforceIf(b)
	model.AddLessThan(x, model.NewConstant(5)).OnlyEnforceIf(b.Not())

	// Create our two half-reified constraints.
	// First, b implies (y == 10 - x).
	model.AddEquality(cpmodel.NewLinearExpr().Add(x).Add(y), cpmodel.NewConstant(10)).OnlyEnforceIf(b)
	// Second, b.Not() implies (y == 0).
	model.AddEquality(y, cpmodel.NewConstant(0)).OnlyEnforceIf(b.Not())

	// Search for x values in increasing order.
	model.AddDecisionStrategy([]cpmodel.IntVar{x}, cmpb.DecisionStrategyProto_CHOOSE_FIRST, cmpb.DecisionStrategyProto_SELECT_MIN_VALUE)

	// Create a solver and solve with a fixed search.
	m, err := model.Model()
	if err != nil {
		return fmt.Errorf("failed to instantiate the CP model: %w", err)
	}
	params := &sppb.SatParameters{
		FillAdditionalSolutionsInResponse: proto.Bool(true),
		EnumerateAllSolutions:             proto.Bool(true),
		SolutionPoolSize:                  proto.Int32(11),
		SearchBranching:                   sppb.SatParameters_FIXED_SEARCH.Enum(),
	}
	response, err := cpmodel.SolveCpModelWithParameters(m, params)
	if err != nil {
		return fmt.Errorf("failed to solve the model: %w", err)
	}

	fmt.Printf("Status: %v\n", response.GetStatus())

	for _, additionalSolution := range response.GetAdditionalSolutions() {
		vs := additionalSolution.GetValues()
		fmt.Printf("x: %v y: %v b: %v\n", vs[x.Index()], vs[y.Index()], vs[b.Index()])
	}

	return nil
}

func main() {
	if err := channelingSampleSat(); err != nil {
		log.Exitf("channelingSampleSat returned with error: %v", err)
	}
}
