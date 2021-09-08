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

    public class IntervalVar
    {
        public IntervalVar(CpModelProto model, LinearExpressionProto start, LinearExpressionProto size,
                           LinearExpressionProto end, int is_present_index, string name)
        {
            model_ = model;
            index_ = model.Constraints.Count;
            interval_ = new IntervalConstraintProto();
            interval_.StartView = start;
            interval_.SizeView = size;
            interval_.EndView = end;

            ConstraintProto ct = new ConstraintProto();
            ct.Interval = interval_;
            ct.Name = name;
            ct.EnforcementLiteral.Add(is_present_index);
            model.Constraints.Add(ct);
        }

        public IntervalVar(CpModelProto model, LinearExpressionProto start, LinearExpressionProto size,
                           LinearExpressionProto end, string name)
        {
            model_ = model;
            index_ = model.Constraints.Count;
            interval_ = new IntervalConstraintProto();
            interval_.StartView = start;
            interval_.SizeView = size;
            interval_.EndView = end;

            ConstraintProto ct = new ConstraintProto();
            ct.Interval = interval_;
            ct.Name = name;
            model_.Constraints.Add(ct);
        }

        public int GetIndex()
        {
            return index_;
        }

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
