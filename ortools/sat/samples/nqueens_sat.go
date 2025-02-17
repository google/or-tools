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

// The nqueens_sat command is an OR-Tools solution to the N-queens problem.
package main

import (
	"fmt"

	log "github.com/golang/glog"
	"github.com/google/or-tools/ortools/sat/go/cpmodel"
)

const boardSize = 8

func nQueensSat() error {
	model := cpmodel.NewCpModelBuilder()

	// There are `boardSize` number of variables, one for a queen in each column
	// of the board. The value of each variable is the row that the queen is in.
	var queenRows []cpmodel.LinearArgument
	for i := 0; i < boardSize; i++ {
		queenRows = append(queenRows, model.NewIntVar(0, int64(boardSize-1)))
	}

	// The following sets the constraint that all queens are in different rows.
	model.AddAllDifferent(queenRows...)

	// No two queens can be on the same diagonal.
	var diag1 []cpmodel.LinearArgument
	var diag2 []cpmodel.LinearArgument
	for i := 0; i < boardSize; i++ {
		diag1 = append(diag1, cpmodel.NewConstant(int64(i)).Add(queenRows[i]))
		diag2 = append(diag2, cpmodel.NewConstant(int64(-i)).Add(queenRows[i]))
	}
	model.AddAllDifferent(diag1...)
	model.AddAllDifferent(diag2...)

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
	for i := int64(0); i < boardSize; i++ {
		for j := 0; j < boardSize; j++ {
			if cpmodel.SolutionIntegerValue(response, queenRows[j]) == i {
				fmt.Print("Q")
			} else {
				fmt.Print("_")
			}
		}
		fmt.Println()
	}

	return nil
}

func main() {
	err := nQueensSat()
	if err != nil {
		log.Exitf("nQueensSat returned with error: %v", err)
	}
}
