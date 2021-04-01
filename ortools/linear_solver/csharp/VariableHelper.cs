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

namespace Google.OrTools.LinearSolver
{
    using System;
    using System.Collections.Generic;

    // Patch the MPVariable class to support the natural language API.
    public partial class Variable
    {
        public static LinearExpr operator +(Variable a, double v)
        {
            return new VarWrapper(a) + v;
        }

        public static LinearExpr operator +(double v, Variable a)
        {
            return a + v;
        }

        public static LinearExpr operator +(Variable a, LinearExpr b)
        {
            return new VarWrapper(a) + b;
        }

        public static LinearExpr operator +(Variable a, Variable b)
        {
            return new VarWrapper(a) + new VarWrapper(b);
        }

        public static LinearExpr operator +(LinearExpr a, Variable b)
        {
            return a + new VarWrapper(b);
        }

        public static LinearExpr operator -(Variable a, double v)
        {
            return new VarWrapper(a) - v;
        }

        public static LinearExpr operator -(double v, Variable a)
        {
            return v - new VarWrapper(a);
        }

        public static LinearExpr operator -(Variable a, LinearExpr b)
        {
            return new VarWrapper(a) - b;
        }

        public static LinearExpr operator -(LinearExpr a, Variable b)
        {
            return a - new VarWrapper(b);
        }

        public static LinearExpr operator -(Variable a, Variable b)
        {
            return new VarWrapper(a) - new VarWrapper(b);
        }

        public static LinearExpr operator -(Variable a)
        {
            return -new VarWrapper(a);
        }

        public static LinearExpr operator *(Variable a, double v)
        {
            return new VarWrapper(a) * v;
        }

        public static LinearExpr operator /(Variable a, double v)
        {
            return new VarWrapper(a) / v;
        }

        public static LinearExpr operator *(double v, Variable a)
        {
            return v * new VarWrapper(a);
        }

        public static RangeConstraint operator ==(Variable a, double v)
        {
            return new VarWrapper(a) == v;
        }

        public static RangeConstraint operator ==(double v, Variable a)
        {
            return v == new VarWrapper(a);
        }

        public static RangeConstraint operator !=(Variable a, double v)
        {
            return new VarWrapper(a) != v;
        }

        public static RangeConstraint operator !=(double v, Variable a)
        {
            return new VarWrapper(a) != v;
        }

        public static Equality operator ==(Variable a, LinearExpr b)
        {
            return new VarWrapper(a) == b;
        }

        public static Equality operator ==(LinearExpr a, Variable b)
        {
            return a == new VarWrapper(b);
        }

        public static VarEquality operator ==(Variable a, Variable b)
        {
            return new VarEquality(a, b, true);
        }

        public static Equality operator !=(Variable a, LinearExpr b)
        {
            return new VarWrapper(a) != b;
        }

        public static Equality operator !=(LinearExpr a, Variable b)
        {
            return a != new VarWrapper(b);
        }

        public static VarEquality operator !=(Variable a, Variable b)
        {
            return new VarEquality(a, b, false);
        }

        public static RangeConstraint operator <=(Variable a, double v)
        {
            return new VarWrapper(a) <= v;
        }

        public static RangeConstraint operator >=(Variable a, double v)
        {
            return new VarWrapper(a) >= v;
        }

        public static RangeConstraint operator <=(double v, Variable a)
        {
            return new VarWrapper(a) >= v;
        }

        public static RangeConstraint operator >=(double v, Variable a)
        {
            return new VarWrapper(a) <= v;
        }

        public static RangeConstraint operator <=(Variable a, LinearExpr b)
        {
            return new VarWrapper(a) <= b;
        }

        public static RangeConstraint operator >=(Variable a, LinearExpr b)
        {
            return new VarWrapper(a) >= b;
        }

        public static RangeConstraint operator <=(Variable a, Variable b)
        {
            return new VarWrapper(a) <= new VarWrapper(b);
        }

        public static RangeConstraint operator >=(Variable a, Variable b)
        {
            return new VarWrapper(a) >= new VarWrapper(b);
        }

        public static RangeConstraint operator <=(LinearExpr a, Variable b)
        {
            return a <= new VarWrapper(b);
        }

        public static RangeConstraint operator >=(LinearExpr a, Variable b)
        {
            return a >= new VarWrapper(b);
        }
    }

    // TODO(user): Try to move this code back to the .swig with @define macros.
    public partial class MPVariableVector : IDisposable,
                                            System.Collections.IEnumerable
#if !SWIG_DOTNET_1
        ,
                                            System.Collections.Generic.IList<Variable>
#endif
    {
        // cast from C# MPVariable array
        public static implicit operator MPVariableVector(Variable[] inVal)
        {
            var outVal = new MPVariableVector();
            foreach (Variable element in inVal)
            {
                outVal.Add(element);
            }
            return outVal;
        }

        // cast to C# MPVariable array
        public static implicit operator Variable[](MPVariableVector inVal)
        {
            var outVal = new Variable[inVal.Count];
            inVal.CopyTo(outVal);
            return outVal;
        }
    }

} // namespace Google.OrTools.LinearSolver
