#!/usr/bin/env python3
from ortools.constraint_solver import pywrapcp
from ortools.linear_solver import pywraplp


def main():
  cp = pywrapcp.Solver("test")
  lp = pywraplp.Solver.CreateSolver("GLOP")


if __name__ == "__main__":
  main()
