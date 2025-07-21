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

// The ranking_sample_sat command is an example of ranking intervals in a NoOverlap constraint.
package main

import (
	"fmt"

	log "github.com/golang/glog"
	"github.com/google/or-tools/ortools/sat/go/cpmodel"

	cmpb "github.com/google/or-tools/ortools/sat/proto/cpmodel"
)

const (
	horizonLength = 100
	numTasks      = 4
)

// rankTasks adds constraints and variables to link tasks and ranks. This method
// assumes that all starts are disjoint, meaning that all tasks have a strictly
// positive duration, and they appear in the same NoOverlap constraint.
func rankTasks(starts []cpmodel.IntVar, presences []cpmodel.BoolVar, ranks []cpmodel.IntVar, model *cpmodel.Builder) {
	// Creates precedence variables between pairs of intervals.
	precedences := make([][]cpmodel.BoolVar, numTasks)
	for i := 0; i < numTasks; i++ {
		precedences[i] = make([]cpmodel.BoolVar, numTasks)
		for j := 0; j < numTasks; j++ {
			if i == j {
				precedences[i][i] = presences[i]
			} else {
				prec := model.NewBoolVar()
				precedences[i][j] = prec
				model.AddLessOrEqual(starts[i], starts[j]).OnlyEnforceIf(prec)
			}
		}
	}

	// Treats optional intervals.
	for i := 0; i+1 < numTasks; i++ {
		for j := i + 1; j < numTasks; j++ {
			// Make sure that if task i is not performed, all precedences are false.
			model.AddImplication(presences[i].Not(), precedences[i][j].Not())
			model.AddImplication(presences[i].Not(), precedences[j][i].Not())
			// Make sure that if task j is not performed, all precedences are false.
			model.AddImplication(presences[j].Not(), precedences[i][j].Not())
			model.AddImplication(presences[j].Not(), precedences[j][i].Not())
			// The following BoolOr will enforce that for any two intervals:
			// 		i precedes j or j precedes i or at least one interval is not performed.
			model.AddBoolOr(precedences[i][j], precedences[j][i], presences[i].Not(), presences[j].Not())
			// Redundant constraint: it propagates early that at most one precedence
			// is true.
			model.AddImplication(precedences[i][j], precedences[j][i].Not())
			model.AddImplication(precedences[j][i], precedences[i][j].Not())
		}
	}

	// Links precedences and ranks.
	for i := 0; i < numTasks; i++ {
		sumOfPredecessors := cpmodel.NewConstant(-1)
		for j := 0; j < numTasks; j++ {
			sumOfPredecessors.Add(precedences[j][i])
		}
		model.AddEquality(ranks[i], sumOfPredecessors)
	}
}

func rankingSampleSat() error {
	model := cpmodel.NewCpModelBuilder()

	starts := make([]cpmodel.IntVar, numTasks)
	ends := make([]cpmodel.IntVar, numTasks)
	intervals := make([]cpmodel.IntervalVar, numTasks)
	presences := make([]cpmodel.BoolVar, numTasks)
	ranks := make([]cpmodel.IntVar, numTasks)

	horizon := cpmodel.NewDomain(0, horizonLength)
	possibleRanks := cpmodel.NewDomain(-1, numTasks-1)

	for t := 0; t < numTasks; t++ {
		start := model.NewIntVarFromDomain(horizon)
		duration := cpmodel.NewConstant(int64(t + 1))
		end := model.NewIntVarFromDomain(horizon)
		var presence cpmodel.BoolVar
		if t < numTasks/2 {
			presence = model.TrueVar()
		} else {
			presence = model.NewBoolVar()
		}
		interval := model.NewOptionalIntervalVar(start, duration, end, presence)
		rank := model.NewIntVarFromDomain(possibleRanks)

		starts[t] = start
		ends[t] = end
		intervals[t] = interval
		presences[t] = presence
		ranks[t] = rank
	}

	// Adds NoOverlap constraint.
	model.AddNoOverlap(intervals...)

	// Ranks tasks.
	rankTasks(starts, presences, ranks, model)

	// Adds a constraint on ranks.
	model.AddLessThan(ranks[0], ranks[1])

	// Creates makespan variables.
	makespan := model.NewIntVarFromDomain(horizon)
	for t := 0; t < numTasks; t++ {
		model.AddLessOrEqual(ends[t], makespan).OnlyEnforceIf(presences[t])
	}

	// Create objective: minimize 2 * makespan - 7 * sum of presences.
	// This is you gain 7 by interval performed, but you pay 2 by day of delays.
	objective := cpmodel.NewLinearExpr().AddTerm(makespan, 2)
	for t := 0; t < numTasks; t++ {
		objective.AddTerm(presences[t], -7)
	}
	model.Minimize(objective)

	// Solving part.
	m, err := model.Model()
	if err != nil {
		return fmt.Errorf("failed to instantiate the CP model: %w", err)
	}
	response, err := cpmodel.SolveCpModel(m)
	if err != nil {
		return fmt.Errorf("failed to solve the model: %w", err)
	}

	if response.GetStatus() == cmpb.CpSolverStatus_OPTIMAL {
		fmt.Println(response.GetStatus())
		fmt.Println("Optimal cost: ", response.GetObjectiveValue())
		fmt.Println("Makespan: ", cpmodel.SolutionIntegerValue(response, makespan))
		for t := 0; t < numTasks; t++ {
			rank := cpmodel.SolutionIntegerValue(response, ranks[t])
			if cpmodel.SolutionBooleanValue(response, presences[t]) {
				start := cpmodel.SolutionIntegerValue(response, starts[t])
				fmt.Printf("Task %v starts at %v with rank %v\n", t, start, rank)
			} else {
				fmt.Printf("Task %v is not performed and ranked at %v\n", t, rank)
			}
		}
	}

	return nil
}

func main() {
	if err := rankingSampleSat(); err != nil {
		log.Exitf("rankingSampleSat returned with error: %v", err)
	}
}
