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

// The no_overlap_sample_sat command is an example of the NoOverlap constraints.
package main

import (
	"fmt"

	log "github.com/golang/glog"
	"github.com/google/or-tools/ortools/sat/go/cpmodel"
	cmpb "github.com/google/or-tools/ortools/sat/proto/cpmodel"
)

const horizon = 21 // 3 weeks

func noOverlapSampleSat() error {
	model := cpmodel.NewCpModelBuilder()
	domain := cpmodel.NewDomain(0, horizon)

	// Task 0, duration 2.
	start0 := model.NewIntVarFromDomain(domain)
	duration0 := cpmodel.NewConstant(2)
	end0 := model.NewIntVarFromDomain(domain)
	task0 := model.NewIntervalVar(start0, duration0, end0)

	// Task 1, duration 4.
	start1 := model.NewIntVarFromDomain(domain)
	duration1 := cpmodel.NewConstant(4)
	end1 := model.NewIntVarFromDomain(domain)
	task1 := model.NewIntervalVar(start1, duration1, end1)

	// Task 2, duration 3
	start2 := model.NewIntVarFromDomain(domain)
	duration2 := cpmodel.NewConstant(2)
	end2 := model.NewIntVarFromDomain(domain)
	task2 := model.NewIntervalVar(start2, duration2, end2)

	// Weekends.
	weekend0 := model.NewFixedSizeIntervalVar(cpmodel.NewConstant(5), 2)
	weekend1 := model.NewFixedSizeIntervalVar(cpmodel.NewConstant(12), 2)
	weekend2 := model.NewFixedSizeIntervalVar(cpmodel.NewConstant(19), 2)

	// No Overlap constraint.
	model.AddNoOverlap(task0, task1, task2, weekend0, weekend1, weekend2)

	// Makespan.
	makespan := model.NewIntVarFromDomain(domain)
	model.AddLessOrEqual(end0, makespan)
	model.AddLessOrEqual(end1, makespan)
	model.AddLessOrEqual(end2, makespan)

	model.Minimize(makespan)

	// Solve.
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
		fmt.Println("Optimal Schedule Length: ", response.GetObjectiveValue())
		fmt.Println("Task 0 starts at ", cpmodel.SolutionIntegerValue(response, start0))
		fmt.Println("Task 1 starts at ", cpmodel.SolutionIntegerValue(response, start1))
		fmt.Println("Task 2 starts at ", cpmodel.SolutionIntegerValue(response, start2))
	}

	return nil
}

func main() {
	if err := noOverlapSampleSat(); err != nil {
		log.Exitf("noOverlapSampleSat returned with error: %v", err)
	}
}
