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

// The earliness_tardiness_cost_sample_sat command is an example of an implementation of a convex
// piecewise linear function.
package main

import (
	"fmt"

	log "github.com/golang/glog"
	"github.com/google/or-tools/ortools/sat/go/cpmodel"
	"google.golang.org/protobuf/proto"

	cmpb "github.com/google/or-tools/ortools/sat/proto/cpmodel"
	sppb "github.com/google/or-tools/ortools/sat/proto/satparameters"
)

const (
	earlinessDate = 5
	earlinessCost = 8
	latenessDate  = 15
	latenessCost  = 12
	largeConstant = 1000
)

func earlinessTardinessCostSampleSat() error {
	// Create the CP-SAT model.
	model := cpmodel.NewCpModelBuilder()

	// Declare our primary variable.
	x := model.NewIntVar(0, 20)

	// Create the expression variable and implement the piecewise linear function.
	//
	//  \        /
	//   \______/
	//   ed    ld
	//
	expr := model.NewIntVar(0, largeConstant)

	// Link together expr and x through the 3 segments.
	firstSegment := cpmodel.NewConstant(earlinessDate*earlinessCost).AddTerm(x, -earlinessCost)
	secondSegment := cpmodel.NewConstant(0)
	thirdSegment := cpmodel.NewConstant(-latenessDate*latenessCost).AddTerm(x, latenessCost)
	model.AddMaxEquality(expr, firstSegment, secondSegment, thirdSegment)

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
	if err := earlinessTardinessCostSampleSat(); err != nil {
		log.Exitf("earlinessTardinessCostSampleSat returned with error: %v", err)
	}
}
