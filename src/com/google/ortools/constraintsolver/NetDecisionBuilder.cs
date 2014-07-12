// Copyright 2010-2014 Google
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
using System.Collections;

namespace Google.OrTools.ConstraintSolver
{
/**
 * This class acts as a intermediate step between a c++ decision
 * builder and a .Net one. Its main purpose is to catch the .Net
 * exception launched when a failure occurs during the Next() call,
 * and to return silently a System.ApplicationException that will
 * propagate the failure back to the C++ code.
 *
 */
public class NetDecisionBuilder : DecisionBuilder
{
  /**
   * This methods wraps the calls to next() and catches fail exceptions.
   */
  public override Decision NextWrapper(Solver solver)
  {
    try
    {
      return Next(solver);
    }
    catch (ApplicationException e)
    {
      return solver.MakeFailDecision();
    }
  }
  /**
   * This is the new method to subclass when defining a .Net decision builder.
   */
  public virtual Decision Next(Solver solver)
  {
    return null;
  }
}

public class NetDemon : Demon
{
  /**
   * This methods wraps the calls to next() and catches fail exceptions.
   */
  public override void RunWrapper(Solver solver)
  {
    try
    {
      Run(solver);
    }
    catch (ApplicationException e)
    {
      solver.ShouldFail();
    }
  }
  /**
   * This is the new method to subclass when defining a .Net decision builder.
   */
  public virtual void Run(Solver solver) {}
  public override int Priority() {
    return Solver.NORMAL_PRIORITY;
  }
  public override string ToString() {
    return "NetDemon";
  }
}

public class NetConstraint : Constraint {
  public NetConstraint(Solver s) : base(s) {}

  public override void InitialPropagateWrapper() {
    try {
      InitialPropagate();
    } catch (ApplicationException e) {
      solver().ShouldFail();
    }
  }
  public virtual void InitialPropagate() {}
  public override string ToString() {
    return "NetConstraint";
  }
}

public class IntVarEnumerator : IEnumerator {
  private IntVarIterator iterator_;

  // Enumerators are positioned before the first element
  // until the first MoveNext() call.
  bool first_ = true;

  public IntVarEnumerator(IntVarIterator iterator) {
    iterator_ = iterator;
  }

  public bool MoveNext() {
    if (first_) {
      iterator_.Init();
      first_ = false;
    } else {
      iterator_.Next();
    }
    return iterator_.Ok();
  }

  public void Reset() {
    first_ = true;
  }

  object IEnumerator.Current {
    get {
      return Current;
    }
  }

  public long Current {
    get {
      if (iterator_.Ok()) {
        return iterator_.Value();
      } else {
        throw new InvalidOperationException();
      }
    }
  }
}

public partial class IntVarIterator : BaseObject, IEnumerable {
  IEnumerator IEnumerable.GetEnumerator() {
    return (IEnumerator) GetEnumerator();
  }

  public IntVarEnumerator GetEnumerator() {
    return new IntVarEnumerator(this);
  }
}
}  // namespace Google.OrTools.ConstraintSolver
