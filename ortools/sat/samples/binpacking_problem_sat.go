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

// The binpacking_problem_sat command is an example of a bin packing problem that uses channeling
// constraints.
package main

import (
	"fmt"

	log "github.com/golang/glog"
	"github.com/google/or-tools/ortools/sat/go/cpmodel"
)

const (
	binCapacity   = 100
	slackCapacity = 20
	safeCapacity  = binCapacity - slackCapacity
	numBins       = 5
)

type item struct {
	Cost, Copies int64
}

func binpackingProblemSat() error {
	// Create the CP-SAT model.
	model := cpmodel.NewCpModelBuilder()

	items := []item{{20, 6}, {15, 6}, {30, 4}, {45, 3}}
	numItems := len(items)

	// Main variables.
	x := make([][]cpmodel.IntVar, numItems)
	for i, item := range items {
		x[i] = make([]cpmodel.IntVar, numBins)
		for b := 0; b < numBins; b++ {
			x[i][b] = model.NewIntVar(0, item.Copies)
		}
	}

	// Load variables.
	load := make([]cpmodel.IntVar, numBins)
	for b := 0; b < numBins; b++ {
		load[b] = model.NewIntVar(0, binCapacity)
	}

	// Slack variables.
	slacks := make([]cpmodel.BoolVar, numBins)
	for b := 0; b < numBins; b++ {
		slacks[b] = model.NewBoolVar()
	}

	// Links load and x.
	for b := 0; b < numBins; b++ {
		expr := cpmodel.NewLinearExpr()
		for i := 0; i < numItems; i++ {
			expr.AddTerm(x[i][b], items[i].Cost)
		}
		model.AddEquality(expr, load[b])
	}

	// Place all items.
	for i := 0; i < numItems; i++ {
		copies := cpmodel.NewLinearExpr()
		for _, b := range x[i] {
			copies.Add(b)
		}
		model.AddEquality(copies, cpmodel.NewConstant(items[i].Copies))
	}

	// Links load and slack through an equivalence relation.
	for b := 0; b < numBins; b++ {
		// slacks[b] => load[b] <= safeCapacity.
		model.AddLessOrEqual(load[b], cpmodel.NewConstant(safeCapacity)).OnlyEnforceIf(slacks[b])
		// slacks[b].Not() => load[b] > safeCapacity.
		model.AddGreaterThan(load[b], cpmodel.NewConstant(safeCapacity)).OnlyEnforceIf(slacks[b].Not())
	}

	// Maximize sum of slacks.
	obj := cpmodel.NewLinearExpr()
	for _, s := range slacks {
		obj.Add(s)
	}
	model.Maximize(obj)

	// Solve.
	m, err := model.Model()
	if err != nil {
		return fmt.Errorf("failed to instantiate the CP model: %w", err)
	}
	response, err := cpmodel.SolveCpModel(m)
	if err != nil {
		return fmt.Errorf("failed to solve the model: %w", err)
	}

	fmt.Println("Status: ", response.GetStatus())
	fmt.Println("Objective: ", response.GetObjectiveValue())
	fmt.Println("Statistics: ")
	fmt.Println("  - conflicts : ", response.GetNumConflicts())
	fmt.Println("  - branches  : ", response.GetNumBranches())
	fmt.Println("  - wall time : ", response.GetWallTime())

	return nil
}

func main() {
	if err := binpackingProblemSat(); err != nil {
		log.Exitf("binpackingProblemSat returned with error: %v", err)
	}
}
