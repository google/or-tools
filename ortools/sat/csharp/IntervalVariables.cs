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

namespace Google.OrTools.Sat
{
/**
 * <summary>An interval variable </summary>
 *
 * <remarks>
 * This class must be constructed from the CpModel class.
 * </remarks>
 */
public class IntervalVar
{
    public IntervalVar(CpModelProto model, LinearExpressionProto start, LinearExpressionProto size,
                       LinearExpressionProto end, int is_present_index, string name)
    {
        model_ = model;
        index_ = model.Constraints.Count;
        interval_ = new IntervalConstraintProto();
        interval_.Start = start;
        interval_.Size = size;
        interval_.End = end;

        ConstraintProto ct = new ConstraintProto();
        ct.Interval = interval_;
        ct.Name = name;
        ct.EnforcementLiteral.Capacity = 1;
        ct.EnforcementLiteral.Add(is_present_index);
        model.Constraints.Add(ct);
    }

    public IntervalVar(CpModelProto model, LinearExpressionProto start, LinearExpressionProto size,
                       LinearExpressionProto end, string name)
    {
        model_ = model;
        index_ = model.Constraints.Count;
        interval_ = new IntervalConstraintProto();
        interval_.Start = start;
        interval_.Size = size;
        interval_.End = end;

        ConstraintProto ct = new ConstraintProto();
        ct.Interval = interval_;
        ct.Name = name;
        model_.Constraints.Add(ct);
    }

    /** <summary>The Index of the interval in the model proto</summary> */
    public int GetIndex()
    {
        return index_;
    }

    /** <summary>The start expression of the interval</summary> */
    public LinearExpr StartExpr()
    {
        return LinearExpr.RebuildLinearExprFromLinearExpressionProto(interval_.Start, model_);
    }

    /** <summary>The size expression of the interval</summary> */
    public LinearExpr SizeExpr()
    {
        return LinearExpr.RebuildLinearExprFromLinearExpressionProto(interval_.Size, model_);
    }

    /** <summary>The end expression of the interval</summary> */
    public LinearExpr EndExpr()
    {
        return LinearExpr.RebuildLinearExprFromLinearExpressionProto(interval_.End, model_);
    }

    /** <summary>The underlying interval proto</summary> */
    public IntervalConstraintProto Proto
    {
        get {
            return interval_;
        }
        set {
            interval_ = value;
        }
    }

    public override string ToString()
    {
        return model_.Constraints[index_].ToString();
    }

    public string Name()
    {
        return model_.Constraints[index_].Name;
    }

    private CpModelProto model_;
    private int index_;
    private IntervalConstraintProto interval_;
}

} // namespace Google.OrTools.Sat
