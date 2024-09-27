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

// The nurses_sat command is an example of a simple nurse scheduling problem.
package main

import (
	"fmt"

	log "github.com/golang/glog"
	"github.com/google/or-tools/ortools/sat/go/cpmodel"
)

const (
	numNurses         = 4
	numDays           = 3
	numShifts         = 3
	minShiftsPerNurse = (numShifts * numDays) / numNurses
	maxShiftsPerNurse = (numShifts*numDays-1)/numNurses + 1
)

func nursesSat() error {
	// Creates the model.
	model := cpmodel.NewCpModelBuilder()

	// Creates shift variables.
	// shifts[shiftKey]: nurse 'n' works shift 's' on day 'd'.
	type shiftKey struct {
		nurse int
		shift int
		day   int
	}
	shifts := make(map[shiftKey]cpmodel.BoolVar)
	for n := 1; n <= numNurses; n++ {
		for d := 1; d <= numDays; d++ {
			for s := 1; s <= numShifts; s++ {
				key := shiftKey{nurse: n, shift: s, day: d}
				shifts[key] = model.NewBoolVar().WithName(fmt.Sprintf("shift_n%vd%vs%v", n, d, s))
			}
		}
	}

	// Each shift is assigned to exactly one nurse in the schedule period.
	for d := 1; d <= numDays; d++ {
		for s := 1; s <= numShifts; s++ {
			var nurses []cpmodel.BoolVar
			for n := 1; n <= numNurses; n++ {
				nurses = append(nurses, shifts[shiftKey{nurse: n, shift: s, day: d}])
			}
			model.AddExactlyOne(nurses...)
		}
	}

	// Each nurse works at most one shift per day.
	for n := 1; n <= numNurses; n++ {
		for d := 1; d <= numDays; d++ {
			var work []cpmodel.BoolVar
			for s := 1; s <= numShifts; s++ {
				work = append(work, shifts[shiftKey{nurse: n, shift: s, day: d}])
			}
			model.AddAtMostOne(work...)
		}
	}

	// Try to distribute the shifts evenly so that each nurse works `minShiftsPerNurse` shifts.
	// If this is not possible because the total number of shifts is not divisible by the number
	// of nurses, some nurses will be assigned one more shift.
	for n := 1; n <= numNurses; n++ {
		shiftsWorked := cpmodel.NewLinearExpr()
		for d := 1; d <= numDays; d++ {
			for s := 1; s <= numShifts; s++ {
				shiftsWorked.Add(shifts[shiftKey{nurse: n, shift: s, day: d}])
			}
		}
		model.AddLessOrEqual(cpmodel.NewConstant(minShiftsPerNurse), shiftsWorked)
		model.AddLessOrEqual(shiftsWorked, cpmodel.NewConstant(maxShiftsPerNurse))
	}

	m, err := model.Model()
	if err != nil {
		return fmt.Errorf("failed to instantiate the CP model: %w", err)
	}
	response, err := cpmodel.SolveCpModel(m)
	if err != nil {
		return fmt.Errorf("failed to solve the model: %w", err)
	}

	fmt.Printf("Status: %v\n", response.GetStatus())
	fmt.Printf("Objective: %v\n", response.GetObjectiveValue())

	fmt.Printf("Solution:\n")
	for d := 1; d <= numDays; d++ {
		fmt.Printf("Day %v\n", d)
		for n := 1; n <= numNurses; n++ {
			var isWorking bool
			for s := 1; s <= numShifts; s++ {
				key := shiftKey{nurse: n, shift: s, day: d}
				if cpmodel.SolutionBooleanValue(response, shifts[key]) {
					isWorking = true
					fmt.Printf("  Nurse %v works shift %v\n", n, s)
				}
			}
			if !isWorking {
				fmt.Printf("  Nurse %v does not work\n", n)
			}
		}
	}

	return nil
}

func main() {
	if err := nursesSat(); err != nil {
		log.Exitf("nursesSat returned with error: %v", err)
	}
}
