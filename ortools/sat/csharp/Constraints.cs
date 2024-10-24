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

namespace Google.OrTools.Sat
{
using System;
using System.Collections.Generic;
using System.Linq;

/**
 * <summary>
 * Wrapper around a ConstraintProto.
 * </summary>
 *
 * <remarks>Constraints created by the CpModel class are automatically added to the model. One needs this
 * class to add an enforcement literal to a constraint. </remarks>
 */
public class Constraint
{
    public Constraint(CpModelProto model)
    {
        index_ = model.Constraints.Count;
        constraint_ = new ConstraintProto();
        model.Constraints.Add(constraint_);
    }

    /** <summary>Adds a literal to the constraint.</summary> */
    public void OnlyEnforceIf(ILiteral lit)
    {
        constraint_.EnforcementLiteral.Add(lit.GetIndex());
    }

    /** <summary>Adds a list of literals to the constraint.</summary> */
    public void OnlyEnforceIf(ILiteral[] lits)
    {
        foreach (ILiteral lit in lits)
        {
            constraint_.EnforcementLiteral.Add(lit.GetIndex());
        }
    }

    /** <summary>The index of the constraint in the model.</summary> */
    public int Index
    {
        get {
            return index_;
        }
    }

    /** <summary>The underlying constraint proto.</summary> */
    public ConstraintProto Proto
    {
        get {
            return constraint_;
        }
        set {
            constraint_ = value;
        }
    }

    private int index_;
    private ConstraintProto constraint_;
}

/**
 *<summary>
 * Specialized circuit constraint.
 * </summary>
 *
 * <remarks>
 * This constraint allows adding arcs to the circuit constraint incrementally.
 * </remarks>
 */
public class CircuitConstraint : Constraint
{
    public CircuitConstraint(CpModelProto model) : base(model)
    {
    }

    /**
     * <summary>
     * Add an arc to the graph of the circuit constraint.
     * </summary>
     *
     * <param name="tail"> the index of the tail node</param>
     * <param name="head"> the index of the head node</param>
     * <param name="literal"> it will be set to true if the arc is selected</param>
     */
    public CircuitConstraint AddArc(int tail, int head, ILiteral literal)
    {
        CircuitConstraintProto circuit = Proto.Circuit;
        circuit.Tails.Add(tail);
        circuit.Heads.Add(head);
        circuit.Literals.Add(literal.GetIndex());
        return this;
    }
}

/**
 *<summary>
 * Specialized multiple circuit constraint.
 * </summary>
 *
 * <remarks>
 * This constraint allows adding arcs to the multiple circuit constraint incrementally.
 * </remarks>
 */
public class MultipleCircuitConstraint : Constraint
{
    public MultipleCircuitConstraint(CpModelProto model) : base(model)
    {
    }

    /**
     * <summary>
     * Add an arc to the graph of the multiple circuit constraint.
     * </summary>
     *
     * <param name="tail"> the index of the tail node</param>
     * <param name="head"> the index of the head node</param>
     * <param name="literal"> it will be set to true if the arc is selected</param>
     */
    public MultipleCircuitConstraint AddArc(int tail, int head, ILiteral literal)
    {
        RoutesConstraintProto routes = Proto.Routes;
        routes.Tails.Add(tail);
        routes.Heads.Add(head);
        routes.Literals.Add(literal.GetIndex());
        return this;
    }
}

/**
 * <summary>
 * Specialized assignment constraint.
 * </summary>
 *
 * <remarks>This constraint allows adding tuples to the allowed/forbidden assignment constraint
 * incrementally.</remarks>
 */
public class TableConstraint : Constraint
{
    public TableConstraint(CpModelProto model) : base(model)
    {
    }

    /**
     * <summary>
     * Adds a tuple of possible/forbidden values to the constraint.
     * </summary>
     *
     * <param name="tuple"> the tuple to add to the constraint</param>
     * <exception cref="ArgumentException"> if the tuple does not have the same length as the array of
     *     variables of the constraint</exception>
     */
    public TableConstraint AddTuple(IEnumerable<int> tuple)
    {
        TableConstraintProto table = Proto.Table;

        int count = 0;
        foreach (int value in tuple)
        {
            table.Values.Add(value);
            count++;
        }
        if (count != table.Exprs.Count)
        {
            throw new ArgumentException("addTuple", "tuple does not have the same length as the variables");
        }
        return this;
    }

    /**
     * <summary>
     * Adds a tuple of possible/forbidden values to the constraint.
     * </summary>
     *
     * <param name="tuple"> the tuple to add to the constraint</param>
     * <exception cref="ArgumentException"> if the tuple does not have the same length as the array of
     *     variables of the constraint</exception>
     */
    public TableConstraint AddTuple(IEnumerable<long> tuple)
    {
        TableConstraintProto table = Proto.Table;

        int count = 0;
        foreach (long value in tuple)
        {
            table.Values.Add(value);
            count++;
        }
        if (count != table.Exprs.Count)
        {
            throw new ArgumentException("addTuple", "tuple does not have the same length as the variables");
        }
        return this;
    }

    /**
     * <summary>
     * Adds a set of tuples of possible/forbidden values to the constraint.
     * </summary>
     *
     * <param name="tuples"> the set of tuple to add to the constraint</param>
     * <exception cref="ArgumentException"> if the tuple does not have the same length as the array of
     *     variables of the constraint</exception>
     */
    public TableConstraint AddTuples(int[,] tuples)
    {
        TableConstraintProto table = Proto.Table;

        if (tuples.GetLength(1) != table.Exprs.Count)
        {
            throw new ArgumentException("addTuples", "tuples does not have the same length as the variables");
        }

        for (int i = 0; i < tuples.GetLength(0); ++i)
        {
            for (int j = 0; j < tuples.GetLength(1); ++j)
            {
                table.Values.Add(tuples[i, j]);
            }
        }
        return this;
    }

    /**
     * <summary>
     * Adds a set of tuples of possible/forbidden values to the constraint.
     * </summary>
     *
     * <param name="tuples"> the set of tuple to add to the constraint</param>
     * <exception cref="ArgumentException"> if the tuple does not have the same length as the array of
     *     variables of the constraint</exception>
     */
    public TableConstraint AddTuples(long[,] tuples)
    {
        TableConstraintProto table = Proto.Table;

        if (tuples.GetLength(1) != table.Exprs.Count)
        {
            throw new ArgumentException("addTuples", "tuples does not have the same length as the variables");
        }

        for (int i = 0; i < tuples.GetLength(0); ++i)
        {
            for (int j = 0; j < tuples.GetLength(1); ++j)
            {
                table.Values.Add(tuples[i, j]);
            }
        }
        return this;
    }
}

/**
 * <summary>
 * Specialized automaton constraint.
 * </summary>
 *
 * <remarks>
 * This constraint allows adding transitions to the automaton constraint incrementally.
 * </remarks>
 */
public class AutomatonConstraint : Constraint
{
    public AutomatonConstraint(CpModelProto model) : base(model)
    {
    }

    /*
     * <summary>
     * Adds a transitions to the automaton.
     * </summary>
     */
    public AutomatonConstraint AddTransition(int tail, int head, long label)
    {
        AutomatonConstraintProto aut = Proto.Automaton;
        aut.TransitionTail.Add(tail);
        aut.TransitionLabel.Add(label);
        aut.TransitionHead.Add(head);
        return this;
    }
}

/**
 * <summary>
 * Specialized reservoir constraint.
 * </summary>
 *
 * <remarks>
 * This constraint allows adding events (time, levelChange, isActive (optional)) to the reservoir
 * constraint incrementally.
 * </remarks>
 */
public class ReservoirConstraint : Constraint
{
    public ReservoirConstraint(CpModel cp_model, CpModelProto model) : base(model)
    {
        this.cp_model_ = cp_model;
    }

    /**
     * <summary>
     * Adds a mandatory event.
     * </summary>
     *
     * <remarks>
     * It will increase the used capacity by `level_change` at time `time`. `time` must be an affine
     * expression.
     * </remarks>
     */
    public ReservoirConstraint AddEvent<T, L>(T time, L level_change)
    {
        ReservoirConstraintProto res = Proto.Reservoir;
        res.TimeExprs.Add(cp_model_.GetLinearExpressionProto(cp_model_.GetLinearExpr(time)));
        res.LevelChanges.Add(cp_model_.GetLinearExpressionProto(cp_model_.GetLinearExpr(level_change)));
        res.ActiveLiterals.Add(cp_model_.TrueLiteral().GetIndex());
        return this;
    }

    /**
     * <summary>
     * Adds an optional event.
     * </summary>
     *
     * <remarks>
     * If `is_active` is true, It will increase the used capacity by `level_change` at time `time`.
     * `time` must be an affine expression.
     * </remarks>
     */
    public ReservoirConstraint AddOptionalEvent<T, L>(T time, L level_change, ILiteral literal)
    {
        ReservoirConstraintProto res = Proto.Reservoir;
        res.TimeExprs.Add(cp_model_.GetLinearExpressionProto(cp_model_.GetLinearExpr(time)));
        res.LevelChanges.Add(cp_model_.GetLinearExpressionProto(cp_model_.GetLinearExpr(level_change)));
        res.ActiveLiterals.Add(literal.GetIndex());
        return this;
    }

    private CpModel cp_model_;
}

/**
 * <summary>
 * Specialized cumulative constraint.
 * </summary>
 *
 * <remarks>
 * This constraint allows adding (interval, demand) pairs to the cumulative constraint
 * incrementally.
 * </remarks>
 */
public class CumulativeConstraint : Constraint
{
    public CumulativeConstraint(CpModel cp_model, CpModelProto model) : base(model)
    {
        this.cp_model_ = cp_model;
    }

    /** <summary>Adds a pair (interval, demand) to the constraint. </summary> */
    public CumulativeConstraint AddDemand<D>(IntervalVar interval, D demand)
    {
        CumulativeConstraintProto cumul = Proto.Cumulative;
        cumul.Intervals.Add(interval.GetIndex());
        LinearExpr demandExpr = cp_model_.GetLinearExpr(demand);
        cumul.Demands.Add(cp_model_.GetLinearExpressionProto(demandExpr));
        return this;
    }

    /** <summary>Adds all pairs (interval, demand) to the constraint. </summary> */
    public CumulativeConstraint AddDemands<D>(IEnumerable<IntervalVar> intervals, IEnumerable<D> demands)
    {
        CumulativeConstraintProto cumul = Proto.Cumulative;
        foreach (var p in intervals.Zip(demands, (i, d) => new { Interval = i, Demand = d }))
        {
            cumul.Intervals.Add(p.Interval.GetIndex());
            LinearExpr demandExpr = cp_model_.GetLinearExpr(p.Demand);
            cumul.Demands.Add(cp_model_.GetLinearExpressionProto(demandExpr));
        }
        return this;
    }

    private CpModel cp_model_;
}

/**
 * <summary>
 * Specialized NoOverlap2D constraint.
 * </summary>
 *
 * <remarks>
 * This constraint allows adding rectangles to the NoOverlap2D constraint incrementally.
 * </remarks>
 */
public class NoOverlap2dConstraint : Constraint
{
    public NoOverlap2dConstraint(CpModelProto model) : base(model)
    {
    }

    /** <summary>Adds a rectangle (xInterval, yInterval) to the constraint. </summary> */
    public NoOverlap2dConstraint AddRectangle(IntervalVar xInterval, IntervalVar yInterval)
    {
        Proto.NoOverlap2D.XIntervals.Add(xInterval.GetIndex());
        Proto.NoOverlap2D.YIntervals.Add(yInterval.GetIndex());
        return this;
    }
}

} // namespace Google.OrTools.Sat
