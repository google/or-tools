# CP/SAT

This directory contains a next-gen Constraint Programming (CP) solver with
clause learning. It is built on top of an efficient SAT/max-SAT solver whose
code is also in this directory.

The technology started in 2009. A complete presentation is available
[here](https://people.eng.unimelb.edu.au/pstuckey/PPDP2013.pdf).

CP-SAT was described during the CPAIOR 2020 masterclass. The recording is
available [here](https://www.youtube.com/watch?v=lmy1ddn4cyw).

To begin, skim
[cp_model.proto](../cp_model.proto) to
understand how optimization problems can be modeled using the solver. You can
then solve a model with the functions in
[cp_model_solver.h](../sat/cp_model_solver.h).


## Parameters

*   [sat_parameters.proto](../sat/sat_parameters.proto):
    The SAT solver parameters. Also contains max-SAT and CP parameters.

## Model

The optimization model description and related utilities:

*   [cp_model.proto](../cp_model.proto):
    Proto describing a general Constraint Programming model.
*   [cp_model_utils.h](../sat/cp_model_utils.h):
    Utilities to manipulate and create a cp_model.proto.
*   [cp_model_checker.h](../sat/cp_model_checker.h):
    Tools to validate cp_model.proto and test a solution feasibility.
*   [cp_model_solver.h](../sat/cp_model_solver.h):
    The solver API.
*   [cp_model_presolve.h](../sat/cp_model_presolve.h):
    Presolve a cp_model.proto by applying simple rules to simplify a subsequent
    solve.

## SAT solver

Stand-alone SAT solver and related files. Note that this is more than a basic
SAT solver as it already includes non-clause constraints. However, these do not
work on the general integer problems that the CP solver handles.

Pure SAT solver:

*   [sat_base.h](../sat/sat_base.h): SAT
    core classes.
*   [clause.h](../sat/clause.h): SAT clause
    propagator with the two-watcher mechanism. Also contains propagators for
    binary clauses.
*   [sat_solver.h](../sat/sat_solver.h):
    The SatSolver code.
*   [simplification.h](../sat/simplification.h):
    SAT postsolver and presolver.
*   [symmetry.h](../sat/symmetry.h):
    Dynamic symmetry breaking constraint in SAT. Not used by default.

Extension:

*   [pb_constraint.h](../sat/pb_constraint.h):
    Implementation of a Pseudo-Boolean constraint propagator for SAT.
    Pseudo-Boolean constraints are simply another name used in the SAT community
    for linear constraints on Booleans.
*   [no_cycle.h](../sat/no_cycle.h):
    Implementation of a no-cycle constraint on a graph whose arc presences are
    controlled by Boolean. This is a SAT propagator, not used in CP.
*   [encoding.h](../sat/encoding.h): Basic
    algorithm to encode integer variables into a binary representation. This is
    not used by the CP solver, just by the max-SAT core based algorithm in
    optimization.h.

Input/output:

*   [drat_writer.h](../sat/drat_writer.h):
    Write UNSAT proof in the DRAT format. This allows to check the correctness
    of an UNSAT proof with the third party program DRAT-trim.
*   [opb_reader.h](../sat/opb_reader.h):
    Parser for the .opb format for Pseudo-Boolean optimization problems.
*   [sat_cnf_reader.h](../sat/sat_cnf_reader.h):
    Parser for the classic SAT .cnf format. Also parses max-SAT files.
*   [boolean_problem.proto](../sat/boolean_problem.proto):
    Deprecated by cp_model.proto.

## CP solver

CP solver built on top of the SAT solver:

*   [integer.h](../sat/integer.h): The
    entry point, which defines the core of the solver.

Basic constraints:

*   [all_different.h](../sat/all_different.h):
    Propagation algorithms for the AllDifferent constraint.
*   [integer_expr.h](../sat/integer_expr.h):
    Propagation algorithms for integer expression (linear, max, min, div, mod,
    ...).
*   [table.h](../sat/table.h): Propagation
    algorithms for the table and automaton constraints.
*   [precedences.h](../sat/precedences.h):
    Propagation algorithms for integer inequalities (integer difference logic
    theory).
*   [cp_constraints.h](../sat/cp_constraints.h):
    Propagation algorithms for other classic CP constraints (XOR, circuit,
    non-overlapping rectangles, ...)
*   [linear_programming_constraint.h](../sat/linear_programming_constraint.h):
    Constraint that solves an LP relaxation of a set of constraints and uses the
    dual-ray to explain an eventual infeasibility. Also implements reduced-cost
    fixing.
*   [flow_costs.h](../sat/flow_costs.h):
    Deprecated. Network flow constraint. We use the generic
    linear_programming_constraint instead.

Scheduling constraints:

*   [intervals.h](../sat/intervals.h):
    Definition and utility for manipulating "interval" variables (a.k.a. task or
    activities). This is the basic CP variable type used in scheduling problems.
*   [disjunctive.h](../sat/disjunctive.h):
    Propagation algorithms for the disjunctive scheduling constraint.
*   [cumulative.h](../sat/cumulative.h),
    [timetable.h](../sat/timetable.h),
    [timetable_edgefinding.h](../sat/timetable_edgefinding.h):
    Propagation algorithms for the cumulative scheduling constraint.
*   [cumulative_energy.h](../sat/cumulative_energy.h):
    Propagation algorithms for a more general cumulative constraint.
*   [theta_tree.h](../sat/theta_tree.h):
    Data structure used in the cumulative/disjunctive propagation algorithm.

## Other

*   [model.h](../sat/model.h): Generic
    class that implements a basic dependency injection framework for singleton
    objects and manages the memory ownership of all the solver classes.
*   [optimization.h](../sat/optimization.h):
    Algorithms to solve an optimization problem using a satisfiability solver as
    a black box.
*   [lp_utils.h](../sat/lp_utils.h):
    Utility to scale and convert a MIP model into CP.

## Recipes

You can find a set a code recipes in
[the documentation directory](doc/README.md).
