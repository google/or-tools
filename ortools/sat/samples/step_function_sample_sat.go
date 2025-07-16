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

// The step_function_sample_sat command is an example of an implementation of a step function.
package main

import (
	"fmt"

	log "github.com/golang/glog"
	"github.com/google/or-tools/ortools/sat/go/cpmodel"
	"google.golang.org/protobuf/proto"

	cmpb "github.com/google/or-tools/ortools/sat/proto/cpmodel"
	sppb "github.com/google/or-tools/ortools/sat/proto/satparameters"
)

func stepFunctionSampleSat() error {
	// Create the CP-SAT model.
	model := cpmodel.NewCpModelBuilder()

	// Declare our primary variable.
	x := model.NewIntVar(0, 20)

	// Create the expression variable and implement the step function
	// Note it is not defined for var == 2.
	//
	//        -               3
	// -- --      ---------   2
	//                        1
	//      -- ---            0
	//           1         2
	// 012345678901234567890
	//
	expr := model.NewIntVar(0, 3)

	// expr == 0 on [5, 6] U [8, 10]
	b0 := model.NewBoolVar()
	d0 := cpmodel.FromValues([]int64{5, 6, 8, 9, 10})
	model.AddLinearConstraintForDomain(x, d0).OnlyEnforceIf(b0)
	model.AddEquality(expr, cpmodel.NewConstant(0)).OnlyEnforceIf(b0)

	// expr == 2 on [0, 1] U [3, 4] U [11, 20]
	b2 := model.NewBoolVar()
	d2 := cpmodel.FromIntervals([]cpmodel.ClosedInterval{{Start: 0, End: 1}, {Start: 3, End: 4}, {Start: 11, End: 20}})
	model.AddLinearConstraintForDomain(x, d2).OnlyEnforceIf(b2)
	model.AddEquality(expr, cpmodel.NewConstant(2)).OnlyEnforceIf(b2)

	// expr = 3 when x = 7
	b3 := model.NewBoolVar()
	model.AddEquality(x, cpmodel.NewConstant(7)).OnlyEnforceIf(b3)
	model.AddEquality(expr, cpmodel.NewConstant(3)).OnlyEnforceIf(b3)

	// At least one Boolean variable is true.
	model.AddBoolOr(b0, b2, b3)

	// Search for x values in increasing order.
	model.AddDecisionStrategy([]cpmodel.IntVar{x}, cmpb.DecisionStrategyProto_CHOOSE_FIRST, cmpb.DecisionStrategyProto_SELECT_MIN_VALUE)

	// Create a solver and solve with fixed search.
	m, err := model.Model()
	if err != nil {
		return fmt.Errorf("failed to instantiate the CP model: %w", err)
	}
	params := &sppb.SatParameters{
		FillAdditionalSolutionsInResponse: proto.Bool(true),
		EnumerateAllSolutions:             proto.Bool(true),
		SolutionPoolSize:                  proto.Int32(21),
		SearchBranching:                   sppb.SatParameters_FIXED_SEARCH.Enum(),
	}
	response, err := cpmodel.SolveCpModelWithParameters(m, params)
	if err != nil {
		return fmt.Errorf("failed to solve the model: %w", err)
	}

	fmt.Printf("Status: %v\n", response.GetStatus())

	for _, additionalSolution := range response.GetAdditionalSolutions() {
		vs := additionalSolution.GetValues()
		fmt.Printf("x= %v expr= %v\n", vs[x.Index()], vs[expr.Index()])
	}

	return nil
}

func main() {
	if err := stepFunctionSampleSat(); err != nil {
		log.Exitf("stepFunctionSampleSat returned with error: %v", err)
	}
}
