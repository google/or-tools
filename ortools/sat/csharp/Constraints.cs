// Copyright 2010-2021 Google LLC
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

public class Constraint
{
    public Constraint(CpModelProto model)
    {
        index_ = model.Constraints.Count;
        constraint_ = new ConstraintProto();
        model.Constraints.Add(constraint_);
    }

    public void OnlyEnforceIf(ILiteral lit)
    {
        constraint_.EnforcementLiteral.Add(lit.GetIndex());
    }

    public void OnlyEnforceIf(ILiteral[] lits)
    {
        foreach (ILiteral lit in lits)
        {
            constraint_.EnforcementLiteral.Add(lit.GetIndex());
        }
    }

    public int Index
    {
        get {
            return index_;
        }
    }

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
 * Specialized circuit constraint.
 *
 * <p>This constraint allows adding arcs to the circuit constraint incrementally.
 */
public class CircuitConstraint : Constraint
{
    public CircuitConstraint(CpModelProto model) : base(model)
    {
    }

    /**
     * Add an arc to the graph of the circuit constraint.
     *
     * @param tail the index of the tail node.
     * @param head the index of the head node.
     * @param literal it will be set to true if the arc is selected.
     */
    public void AddArc(int tail, int head, ILiteral literal)
    {
        CircuitConstraintProto circuit = Proto.Circuit;
        circuit.Tails.Add(tail);
        circuit.Heads.Add(head);
        circuit.Literals.Add(literal.GetIndex());
    }
}

/**
 * Specialized multiple circuit constraint.
 *
 * <p>This constraint allows adding arcs to the multiple circuit constraint incrementally.
 */
public class MultipleCircuitConstraint : Constraint
{
    public MultipleCircuitConstraint(CpModelProto model) : base(model)
    {
    }

    /**
     * Add an arc to the graph of the circuit constraint.
     *
     * @param tail the index of the tail node.
     * @param head the index of the head node.
     * @param literal it will be set to true if the arc is selected.
     */
    public void AddArc(int tail, int head, ILiteral literal)
    {
        RoutesConstraintProto routes = Proto.Routes;
        routes.Tails.Add(tail);
        routes.Heads.Add(head);
        routes.Literals.Add(literal.GetIndex());
    }
}

/**
 * Specialized assignment constraint.
 *
 * <p>This constraint allows adding tuples to the allowed/forbidden assignment constraint
 * incrementally.
 */
public class TableConstraint : Constraint
{
    public TableConstraint(CpModelProto model) : base(model)
    {
    }

    /**
     * Adds a tuple of possible/forbidden values to the constraint.
     *
     * @param tuple the tuple to add to the constraint.
     * @throws CpModel.WrongLength if the tuple does not have the same length as the array of
     *     variables of the constraint.
     */
    public void AddTuple(IEnumerable<int> tuple)
    {
        TableConstraintProto table = Proto.Table;

        int count = 0;
        foreach (int value in tuple)
        {
            table.Values.Add(value);
            count++;
        }
        if (count != table.Vars.Count)
        {
            throw new ArgumentException("addTuple", "tuple does not have the same length as the variables");
        }
    }

    /**
     * Adds a tuple of possible/forbidden values to the constraint.
     *
     * @param tuple the tuple to add to the constraint.
     * @throws CpModel.WrongLength if the tuple does not have the same length as the array of
     *     variables of the constraint.
     */
    public void AddTuple(IEnumerable<long> tuple)
    {
        TableConstraintProto table = Proto.Table;

        int count = 0;
        foreach (long value in tuple)
        {
            table.Values.Add(value);
            count++;
        }
        if (count != table.Vars.Count)
        {
            throw new ArgumentException("addTuple", "tuple does not have the same length as the variables");
        }
    }
}

/**
 * Specialized automaton constraint.
 *
 * <p>This constraint allows adding transitions to the automaton constraint incrementally.
 */
public class AutomatonConstraint : Constraint
{
    public AutomatonConstraint(CpModelProto model) : base(model)
    {
    }

    /// Adds a transitions to the automaton.
    public void AddTransition(int tail, int head, long label)
    {
        AutomatonConstraintProto aut = Proto.Automaton;
        aut.TransitionTail.Add(tail);
        aut.TransitionLabel.Add(label);
        aut.TransitionHead.Add(head);
    }
}

/**
 * Specialized reservoir constraint.
 *
 * <p>This constraint allows adding events (time, levelChange, isActive (optional)) to the reservoir
 * constraint incrementally.
 */
public class ReservoirConstraint : Constraint
{
    public ReservoirConstraint(CpModel cp_model, CpModelProto model) : base(model)
    {
        this.cp_model_ = cp_model;
    }

    /**
     * Adds a mandatory event
     *
     * <p>It will increase the used capacity by `levelChange` at time `time`. `time` must be an affine
     * expression.
     */
    public void AddEvent<T, L>(T time, L level_change)
    {
        ReservoirConstraintProto res = Proto.Reservoir;
        res.TimeExprs.Add(cp_model_.GetLinearExpressionProto(cp_model_.GetLinearExpr(time)));
        res.LevelChanges.Add(Convert.ToInt64(level_change));
        res.ActiveLiterals.Add(cp_model_.TrueLiteral().GetIndex());
    }

    /**
     * Adds an optional event
     *
     * <p>If `isActive` is true, It will increase the used capacity by `levelChange` at time `time`.
     * `time` must be an affine expression.
     */
    public void AddOptionalEvent<T, L>(T time, L level_change, ILiteral literal)
    {
        ReservoirConstraintProto res = Proto.Reservoir;
        res.TimeExprs.Add(cp_model_.GetLinearExpressionProto(cp_model_.GetLinearExpr(time)));
        res.LevelChanges.Add(Convert.ToInt64(level_change));
        res.ActiveLiterals.Add(literal.GetIndex());
    }

    private CpModel cp_model_;
}

/**
 * Specialized cumulative constraint.
 *
 * <p>This constraint allows adding (interval, demand) pairs to the cumulative constraint
 * incrementally.
 */
public class CumulativeConstraint : Constraint
{
    public CumulativeConstraint(CpModel cp_model, CpModelProto model) : base(model)
    {
        this.cp_model_ = cp_model;
    }

    /// Adds a pair (interval, demand) to the constraint.
    public void AddDemand<D>(IntervalVar interval, D demand)
    {
        CumulativeConstraintProto cumul = Proto.Cumulative;
        cumul.Intervals.Add(interval.GetIndex());
        LinearExpr demandExpr = cp_model_.GetLinearExpr(demand);
        cumul.Demands.Add(cp_model_.GetLinearExpressionProto(demandExpr));
    }

    private CpModel cp_model_;
}

/**
 * Specialized NoOverlap2D constraint.
 *
 * <p>This constraint allows adding rectanles to the NoOverlap2D constraint incrementally.
 */
public class NoOverlap2dConstraint : Constraint
{
    public NoOverlap2dConstraint(CpModelProto model) : base(model)
    {
    }

    /// Adds a rectangle (xInterval, yInterval) to the constraint.
    public void AddRectangle(IntervalVar xInterval, IntervalVar yInterval)
    {
        Proto.NoOverlap2D.XIntervals.Add(xInterval.GetIndex());
        Proto.NoOverlap2D.YIntervals.Add(yInterval.GetIndex());
    }
}

} // namespace Google.OrTools.Sat
