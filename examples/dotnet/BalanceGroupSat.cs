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

using System;
using System.Collections.Generic;
using System.Linq;
using Google.OrTools.Sat;

/// <summary>
/// We are trying to group items in equal sized groups.
/// Each item has a color and a value. We want the sum of values of each group
/// to be as close to the average as possible. Furthermore, if one color is an a
/// group, at least k items with this color must be in that group.
/// </summary>
public class BalanceGroupSat
{
    static void Main(string[] args)
    {
        int numberGroups = 10;
        int numberItems = 100;
        int numberColors = 3;
        int minItemsOfSameColorPerGroup = 4;

        var allGroups = Enumerable.Range(0, numberGroups).ToArray();
        var allItems = Enumerable.Range(0, numberItems).ToArray();
        var allColors = Enumerable.Range(0, numberColors).ToArray();

        var values = allItems.Select(i => 1 + i + (i * i / 200)).ToArray();
        var colors = allItems.Select(i => i % numberColors).ToArray();

        var sumOfValues = values.Sum();
        var averageSumPerGroup = sumOfValues / numberGroups;
        var numItemsPerGroup = numberItems / numberGroups;

        var itemsPerColor = new Dictionary<int, List<int>>();

        foreach (var color in allColors)
        {
            itemsPerColor[color] = new List<int>();
            foreach (var item in allItems)
            {
                if (colors[item] == color)
                    itemsPerColor[color].Add(item);
            }
        }

        Console.WriteLine($"Model has {numberItems}, {numberGroups} groups and {numberColors} colors");
        Console.WriteLine($"    Average sum per group = {averageSumPerGroup}");

        var model = new CpModel();

        var itemInGroup = new IntVar[numberItems, numberGroups];
        foreach (var item in allItems)
        {
            foreach (var @group in allGroups)
            {
                itemInGroup[item, @group] = model.NewBoolVar($"item {item} in group {@group}");
            }
        }

        // Each group must have the same size.
        foreach (var @group in allGroups)
        {
            var itemsInGroup = allItems.Select(x => itemInGroup[x, @group]).ToArray();
            model.AddLinearConstraint(LinearExpr.Sum(itemsInGroup), numItemsPerGroup, numItemsPerGroup);
        }

        //# One item must belong to exactly one group.
        foreach (var item in allItems)
        {
            var groupsForItem = allGroups.Select(x => itemInGroup[item, x]).ToArray();
            model.Add(LinearExpr.Sum(groupsForItem) == 1);
        }

        // The deviation of the sum of each items in a group against the average.
        var e = model.NewIntVar(0, 550, "epsilon");

        // Constrain the sum of values in one group around the average sum per
        // group.
        foreach (var @group in allGroups)
        {
            var itemValues = allItems.Select(x => itemInGroup[x, @group]).ToArray();

            var sum = LinearExpr.ScalProd(itemValues, values);
            model.Add(sum <= averageSumPerGroup + e);
            model.Add(sum >= averageSumPerGroup - e);
        }

        // colorInGroup variables.
        var colorInGroup = new IntVar[numberColors, numberGroups];
        foreach (var @group in allGroups)
        {
            foreach (var color in allColors)
            {
                colorInGroup[color, @group] = model.NewBoolVar($"color {color} is in group {@group}");
            }
        }

        // Item is in a group implies its color is in that group.
        foreach (var item in allItems)
        {
            foreach (var @group in allGroups)
            {
                model.AddImplication(itemInGroup[item, @group], colorInGroup[colors[item], @group]);
            }
        }

        // If a color is in a group, it must contains at least
        // min_items_of_same_color_per_group items from that color.
        foreach (var color in allColors)
        {
            foreach (var @group in allGroups)
            {
                var literal = colorInGroup[color, @group];
                var items = itemsPerColor[color].Select(x => itemInGroup[x, @group]).ToArray();
                model.Add(LinearExpr.Sum(items) >= minItemsOfSameColorPerGroup).OnlyEnforceIf(literal);
            }
        }

        // Compute the maximum number of colors in a group.
        int maxColor = numItemsPerGroup / minItemsOfSameColorPerGroup;
        // Redundant constraint: The problem does not solve in reasonable time
        // without it.
        if (maxColor < numberColors)
        {
            foreach (var @group in allGroups)
            {
                var all = allColors.Select(x => colorInGroup[x, @group]).ToArray();
                model.Add(LinearExpr.Sum(all) <= maxColor);
            }
        }

        // Minimize epsilon
        model.Minimize(e);

        var solver = new CpSolver();

        var solutionPrinter = new SolutionPrinter(values, colors, allGroups, allItems, itemInGroup);

        var status = solver.Solve(model, solutionPrinter);
    }

    public class SolutionPrinter : CpSolverSolutionCallback
    {
        private int[] _values;
        private int[] _colors;
        private int[] _allGroups;
        private int[] _allItems;
        private IntVar[,] _itemInGroup;

        private int _solutionCount;

        public SolutionPrinter(int[] values, int[] colors, int[] allGroups, int[] allItems, IntVar[,] itemInGroup)
        {
            this._values = values;
            this._colors = colors;
            this._allGroups = allGroups;
            this._allItems = allItems;
            this._itemInGroup = itemInGroup;
        }

        public override void OnSolutionCallback()
        {
            Console.WriteLine($"Solution {_solutionCount}");
            _solutionCount++;

            Console.WriteLine($"    objective value = {this.ObjectiveValue()}");

            Dictionary<int, List<int>> groups = new Dictionary<int, List<int>>();
            int[] sum = new int[_allGroups.Length];

            foreach (var @group in _allGroups)
            {
                groups[@group] = new List<int>();
                foreach (var item in _allItems)
                {
                    if (BooleanValue(_itemInGroup[item, @group]))
                    {
                        groups[@group].Add(item);
                        sum[@group] += _values[item];
                    }
                }
            }

            foreach (var g in _allGroups)
            {
                var group = groups[g];
                Console.Write($"Group {g}: sum = {sum[g]} [");
                foreach (var item in group)
                {
                    Console.Write($"({item}, {_values[item]}, {_colors[item]})");
                }

                Console.WriteLine("]");
            }
        }
    }
}
