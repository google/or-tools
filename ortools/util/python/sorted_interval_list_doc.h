// Copyright 2010-2022 Google LLC
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

#ifndef OR_TOOLS_UTIL_PYTHON_SORTED_INTERVAL_LIST_DOC_H_
#define OR_TOOLS_UTIL_PYTHON_SORTED_INTERVAL_LIST_DOC_H_

/*
  This file contains docstrings for use in the Python bindings.
  Do not edit! They were automatically extracted by pybind11_mkdoc.
 */

#define __EXPAND(x) x
#define __COUNT(_1, _2, _3, _4, _5, _6, _7, COUNT, ...) COUNT
#define __VA_SIZE(...) __EXPAND(__COUNT(__VA_ARGS__, 7, 6, 5, 4, 3, 2, 1, 0))
#define __CAT1(a, b) a##b
#define __CAT2(a, b) __CAT1(a, b)
#define __DOC1(n1) __doc_##n1
#define __DOC2(n1, n2) __doc_##n1##_##n2
#define __DOC3(n1, n2, n3) __doc_##n1##_##n2##_##n3
#define __DOC4(n1, n2, n3, n4) __doc_##n1##_##n2##_##n3##_##n4
#define __DOC5(n1, n2, n3, n4, n5) __doc_##n1##_##n2##_##n3##_##n4##_##n5
#define __DOC6(n1, n2, n3, n4, n5, n6) \
  __doc_##n1##_##n2##_##n3##_##n4##_##n5##_##n6
#define __DOC7(n1, n2, n3, n4, n5, n6, n7) \
  __doc_##n1##_##n2##_##n3##_##n4##_##n5##_##n6##_##n7
#define DOC(...) \
  __EXPAND(__EXPAND(__CAT2(__DOC, __VA_SIZE(__VA_ARGS__)))(__VA_ARGS__))

#if defined(__GNUG__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-variable"
#endif

static const char* __doc_operations_research_ClosedInterval =
    R"doc(Represents a closed interval [start, end]. We must have start <= end.)doc";

static const char* __doc_operations_research_ClosedInterval_ClosedInterval =
    R"doc()doc";

static const char* __doc_operations_research_ClosedInterval_ClosedInterval_2 =
    R"doc()doc";

static const char* __doc_operations_research_ClosedInterval_DebugString =
    R"doc()doc";

static const char* __doc_operations_research_ClosedInterval_end = R"doc()doc";

static const char* __doc_operations_research_ClosedInterval_operator_eq =
    R"doc()doc";

static const char* __doc_operations_research_ClosedInterval_operator_lt =
    R"doc()doc";

static const char* __doc_operations_research_ClosedInterval_start = R"doc()doc";

static const char* __doc_operations_research_Domain =
    R"doc(We call *domain* any subset of Int64 = [kint64min, kint64max].

This class can be used to represent such set efficiently as a sorted
and non-adjacent list of intervals. This is efficient as long as the
size of such list stays reasonable.

In the comments below, the domain of *this will always be written 'D'.
Note that all the functions are safe with respect to integer overflow.)doc";

static const char* __doc_operations_research_Domain_AdditionWith =
    R"doc(Returns {x ∈ Int64, ∃ a ∈ D, ∃ b ∈ domain, x = a + b}.)doc";

static const char* __doc_operations_research_Domain_AllValues =
    R"doc(Returns the full domain Int64.)doc";

static const char* __doc_operations_research_Domain_ClosestValue =
    R"doc(Returns the value closest to the given point. If there is a tie, pick
larger one.)doc";

static const char* __doc_operations_research_Domain_Complement =
    R"doc(Returns the set Int64 ∖ D.)doc";

static const char* __doc_operations_research_Domain_Contains =
    R"doc(Returns true iff value is in Domain.)doc";

static const char* __doc_operations_research_Domain_ContinuousMultiplicationBy =
    R"doc(Returns a superset of MultiplicationBy() to avoid the explosion in the
representation size. This behaves as if we replace the set D of non-
adjacent integer intervals by the set of floating-point elements in
the same intervals.

For instance, [1, 100] * 2 will be transformed in [2, 200] and not in
[2][4][6]...[200] like in MultiplicationBy(). Note that this would be
similar to a InverseDivisionBy(), but not quite the same because if we
look for {x ∈ Int64, ∃ e ∈ D, x / coeff = e}, then we will get [2,
201] in the case above.)doc";

static const char*
    __doc_operations_research_Domain_ContinuousMultiplicationBy_2 =  // NOLINT
    R"doc(Returns a superset of MultiplicationBy() to avoid the explosion in the
representation size. This behaves as if we replace the set D of non-
adjacent integer intervals by the set of floating-point elements in
the same intervals.

For instance, [1, 100] * 2 will be transformed in [2, 200] and not in
[2][4][6]...[200] like in MultiplicationBy(). Note that this would be
similar to a InverseDivisionBy(), but not quite the same because if we
look for {x ∈ Int64, ∃ e ∈ D, x / coeff = e}, then we will get [2,
201] in the case above.)doc";

static const char* __doc_operations_research_Domain_Distance =
    R"doc(Returns the distance from the value to the domain.)doc";

static const char* __doc_operations_research_Domain_DivisionBy =
    R"doc(Returns {x ∈ Int64, ∃ e ∈ D, x = e / coeff}.

For instance Domain(1, 7).DivisionBy(2) == Domain(0, 3).)doc";

static const char* __doc_operations_research_Domain_Domain =
    R"doc(By default, Domain will be empty.)doc";

static const char* __doc_operations_research_Domain_Domain_2 =
    R"doc(Copy constructor (mandatory as we define the move constructor).)doc";

static const char* __doc_operations_research_Domain_Domain_3 =
    R"doc(Move constructor.)doc";

static const char* __doc_operations_research_Domain_Domain_4 =
    R"doc(Constructor for the common case of a singleton domain.)doc";

static const char* __doc_operations_research_Domain_Domain_5 =
    R"doc(Constructor for the common case of a single interval [left, right]. If
left > right, this will result in the empty domain.)doc";

static const char* __doc_operations_research_Domain_DomainIterator =
    R"doc(Allows to iterate over all values of a domain in order with for (const
int64_t v : domain.Values()) { ... }

Note that this shouldn't be used in another context !! We don't
implement full fledged iterator APIs.)doc";

static const char* __doc_operations_research_Domain_DomainIteratorBeginEnd =
    R"doc()doc";

static const char*
    __doc_operations_research_Domain_DomainIteratorBeginEndWithOwnership =
        R"doc()doc";

static const char*
    __doc_operations_research_Domain_DomainIteratorBeginEndWithOwnership_begin =
        R"doc()doc";

static const char*
    __doc_operations_research_Domain_DomainIteratorBeginEndWithOwnership_const_iterator =  // NOLINT
    R"doc()doc";

static const char*
    __doc_operations_research_Domain_DomainIteratorBeginEnd_begin = R"doc()doc";

static const char*
    __doc_operations_research_Domain_DomainIteratorBeginEnd_const_iterator =
        R"doc()doc";

static const char*
    __doc_operations_research_Domain_DomainIterator_DomainIterator =
        R"doc()doc";

static const char*
    __doc_operations_research_Domain_DomainIterator_const_iterator =
        R"doc()doc";

static const char*
    __doc_operations_research_Domain_DomainIterator_operator_inc = R"doc()doc";

static const char*
    __doc_operations_research_Domain_DomainIterator_operator_mul = R"doc()doc";

static const char* __doc_operations_research_Domain_DomainIterator_operator_ne =
    R"doc()doc";

static const char* __doc_operations_research_Domain_DomainIterator_value =
    R"doc()doc";

static const char* __doc_operations_research_Domain_FixedValue =
    R"doc(Returns the value of a fixed domain. IsFixed() must be true. This is
the same as Min() or Max() but allows for a more readable code and
also crash in debug mode if called on a non fixed domain.)doc";

static const char* __doc_operations_research_Domain_FlattenedIntervals =
    R"doc(This method returns the flattened list of interval bounds of the
domain.

Thus the domain {0, 1, 2, 5, 8, 9, 10} will return [0, 2, 5, 5, 8, 10]
(as a C++ std::vector<int64_t>, as a java or C# long[], as a python
list of integers).)doc";

static const char* __doc_operations_research_Domain_FromFlatIntervals =
    R"doc(This method is available in Python, Java and .NET. It allows building
a Domain object from a flattened list of intervals (long[] in Java and
.NET, [0, 2, 5, 5, 8, 10] in python).)doc";

static const char* __doc_operations_research_Domain_FromFlatSpanOfIntervals =
    R"doc(Same as FromIntervals() for a flattened representation (start, end,
start, end, ...).)doc";

static const char* __doc_operations_research_Domain_FromIntervals =
    R"doc(Creates a domain from the union of an unsorted list of intervals.)doc";

static const char* __doc_operations_research_Domain_FromValues =
    R"doc(Creates a domain from the union of an unsorted list of integer values.
Input values may be repeated, with no consequence on the output)doc";

static const char* __doc_operations_research_Domain_FromVectorIntervals =
    R"doc(This method is available in Python, Java and .NET. It allows building
a Domain object from a list of intervals (long[][] in Java and .NET,
[[0, 2], [5, 5], [8, 10]] in python).)doc";

static const char* __doc_operations_research_Domain_HasTwoValues =
    R"doc(Returns true if the domain has just two values. This often mean a non-
fixed Boolean variable.)doc";

static const char* __doc_operations_research_Domain_IntersectionWith =
    R"doc(Returns the intersection of D and domain.)doc";

static const char* __doc_operations_research_Domain_InverseMultiplicationBy =
    R"doc(Returns {x ∈ Int64, ∃ e ∈ D, x * coeff = e}.

For instance Domain(1, 7).InverseMultiplicationBy(2) == Domain(1, 3).)doc";

static const char* __doc_operations_research_Domain_IsEmpty =
    R"doc(Returns true if this is the empty set.)doc";

static const char* __doc_operations_research_Domain_IsFixed =
    R"doc(Returns true iff the domain is reduced to a single value. The domain
must not be empty.)doc";

static const char* __doc_operations_research_Domain_IsIncludedIn =
    R"doc(Returns true iff D is included in the given domain.)doc";

static const char* __doc_operations_research_Domain_Max =
    R"doc(Returns the max value of the domain. The domain must not be empty.)doc";

static const char* __doc_operations_research_Domain_Min =
    R"doc(Returns the min value of the domain. The domain must not be empty.)doc";

static const char* __doc_operations_research_Domain_MultiplicationBy =
    R"doc(Returns {x ∈ Int64, ∃ e ∈ D, x = e * coeff}.

Note that because the resulting domain will only contains multiple of
coeff, the size of intervals.size() can become really large. If it is
larger than a fixed constant, exact will be set to false and the
result will be set to ContinuousMultiplicationBy(coeff).

Note that if you multiply by a negative coeff, kint64min will be
dropped from the result even if it was here due to how this is
implemented.)doc";

static const char* __doc_operations_research_Domain_Negation =
    R"doc(Returns {x ∈ Int64, ∃ e ∈ D, x = -e}.

Note in particular that if the negation of Int64 is not Int64 but
Int64 \ {kint64min} !!)doc";

static const char* __doc_operations_research_Domain_NumIntervals =
    R"doc(Basic read-only std::vector<> wrapping to view a Domain as a sorted
list of non-adjacent intervals. Note that we don't expose size() which
might be confused with the number of values in the domain.)doc";

static const char* __doc_operations_research_Domain_PositiveDivisionBySuperset =
    R"doc(Returns a superset of {x ∈ Int64, ∃ e ∈ D, ∃ d ∈ divisor, x = e / d }.

We check that divisor is strictly positive. For now we just intersect
with the min/max possible value.)doc";

static const char* __doc_operations_research_Domain_PositiveModuloBySuperset =
    R"doc(Returns a superset of {x ∈ Int64, ∃ e ∈ D, ∃ m ∈ modulo, x = e % m }.

We check that modulo is strictly positive. The sign of the modulo
depends on the sign of e. We compute the exact min/max if the modulo
is fixed, otherwise we will just return a superset.)doc";

static const char* __doc_operations_research_Domain_RelaxIfTooComplex =
    R"doc(If NumIntervals() is too large, this return a superset of the domain.)doc";

static const char* __doc_operations_research_Domain_SimplifyUsingImpliedDomain =
    R"doc(Advanced usage. Given some *implied* information on this domain that
is assumed to be always true (i.e. only values in the intersection
with implied domain matter), this function will simplify the current
domain without changing the set of "possible values".

More precisely, this will: - Take the intersection with
implied_domain. - Minimize the number of intervals. For example, if
the domain is [1,2][4] and implied is [1][4], then the domain can be
relaxed to [1, 4] to simplify its complexity without changing the set
of admissible value assuming only implied values can be seen. -
Restrict as much as possible the bounds of the remaining intervals.
For example, if the input is [1,2] and implied is [0,4], then the
domain will not be changed.

Note that **domain.SimplifyUsingImpliedDomain(domain)** will just
return [domain.Min(), domain.Max()]. This is meant to be applied to
the right-hand side of a constraint to make its propagation more
efficient.)doc";

static const char* __doc_operations_research_Domain_Size =
    R"doc(Returns the number of elements in the domain. It is capped at
kint64max)doc";

static const char* __doc_operations_research_Domain_SmallestValue =
    R"doc(Returns the value closest to zero. If there is a tie, pick positive
one.)doc";

static const char* __doc_operations_research_Domain_SquareSuperset =
    R"doc(Returns a superset of {x ∈ Int64, ∃ y ∈ D, x = y * y }.)doc";

static const char* __doc_operations_research_Domain_ToString =
    R"doc(Returns a compact string of a vector of intervals like
"[1,4][6][10,20]".)doc";

static const char* __doc_operations_research_Domain_UnionWith =
    R"doc(Returns the union of D and domain.)doc";

static const char* __doc_operations_research_Domain_ValueAtOrAfter =
    R"doc()doc";

static const char* __doc_operations_research_Domain_ValueAtOrBefore =
    R"doc(Returns the closest value in the domain that is <= (resp. >=) to the
input. Do not change the input if there is no such value.)doc";

static const char* __doc_operations_research_Domain_Values = R"doc()doc";

static const char* __doc_operations_research_Domain_Values_2 = R"doc()doc";

static const char* __doc_operations_research_Domain_back = R"doc()doc";

static const char* __doc_operations_research_Domain_const_iterator =
    R"doc()doc";

static const char* __doc_operations_research_Domain_front = R"doc()doc";

static const char* __doc_operations_research_Domain_intervals = R"doc()doc";

static const char* __doc_operations_research_Domain_operator_array =
    R"doc()doc";

static const char* __doc_operations_research_Domain_operator_assign =
    R"doc(Copy operator (mandatory as we define the move operator).)doc";

static const char* __doc_operations_research_Domain_operator_assign_2 =
    R"doc(Move operator.)doc";

static const char* __doc_operations_research_Domain_operator_eq = R"doc()doc";

static const char* __doc_operations_research_Domain_operator_lt =
    R"doc(Lexicographic order on the intervals() representation.)doc";

static const char* __doc_operations_research_Domain_operator_ne = R"doc()doc";

static const char* __doc_operations_research_SortedDisjointIntervalList =
    R"doc(This class represents a sorted list of disjoint, closed intervals.
When an interval is inserted, all intervals that overlap it or are
adjacent to it are merged into one. I.e. [0,14] and [15,30] will be
merged to [0,30].

Iterators returned by this class are invalidated by non-const
operations.)doc";

static const char*
    __doc_operations_research_SortedDisjointIntervalList_BuildComplementOnInterval =  // NOLINT
    R"doc(Builds the complement of the interval list on the interval [start,
end].)doc";

static const char*
    __doc_operations_research_SortedDisjointIntervalList_DebugString =
        R"doc()doc";

static const char*
    __doc_operations_research_SortedDisjointIntervalList_FirstIntervalGreaterOrEqual =  // NOLINT
    R"doc(Returns an iterator to either: - the first interval containing or
above the given value, or - the last interval containing or below the
given value. Returns end() if no interval fulfills that condition.

If the value is within an interval, both functions will return it.)doc";

static const char*
    __doc_operations_research_SortedDisjointIntervalList_GrowRightByOne =
        R"doc(If value is in an interval, increase its end by one, otherwise insert
the interval [value, value]. In both cases, this returns an iterator
to the new/modified interval (possibly merged with others) and fills
newly_covered with the new value that was just added in the union of
all the intervals.

If this causes an interval ending at kint64max to grow, it will die
with a CHECK fail.)doc";

static const char*
    __doc_operations_research_SortedDisjointIntervalList_InsertAll =
        R"doc()doc";

static const char*
    __doc_operations_research_SortedDisjointIntervalList_InsertInterval =
        R"doc(Adds the interval [start..end] to the list, and merges overlapping or
immediately adjacent intervals ([2, 5] and [6, 7] are adjacent, but
[2, 5] and [7, 8] are not).

Returns an iterator to the inserted interval (possibly merged with
others).

If start > end, it does LOG(DFATAL) and returns end() (no interval
added).)doc";

static const char*
    __doc_operations_research_SortedDisjointIntervalList_InsertIntervals =
        R"doc(Adds all intervals [starts[i]..ends[i]].

Same behavior as InsertInterval() upon invalid intervals. There's a
version with int64_t and int32_t.)doc";

static const char*
    __doc_operations_research_SortedDisjointIntervalList_InsertIntervals_2 =
        R"doc()doc";

static const char*
    __doc_operations_research_SortedDisjointIntervalList_IntervalComparator =
        R"doc()doc";

static const char*
    __doc_operations_research_SortedDisjointIntervalList_IntervalComparator_operator_call =  // NOLINT
    R"doc()doc";

static const char*
    __doc_operations_research_SortedDisjointIntervalList_LastIntervalLessOrEqual =  // NOLINT
    R"doc()doc";

static const char*
    __doc_operations_research_SortedDisjointIntervalList_NumIntervals =
        R"doc(Returns the number of disjoint intervals in the list.)doc";

static const char*
    __doc_operations_research_SortedDisjointIntervalList_SortedDisjointIntervalList =  // NOLINT
    R"doc()doc";

static const char*
    __doc_operations_research_SortedDisjointIntervalList_SortedDisjointIntervalList_2 =  // NOLINT
    R"doc()doc";

static const char*
    __doc_operations_research_SortedDisjointIntervalList_SortedDisjointIntervalList_3 =  // NOLINT
    R"doc(Creates a SortedDisjointIntervalList and fills it with intervals
[starts[i]..ends[i]]. All intervals must be consistent (starts[i] <=
ends[i]). There are two version, one for int64_t and one for int.)doc";

static const char*
    __doc_operations_research_SortedDisjointIntervalList_SortedDisjointIntervalList_4 =  // NOLINT
    R"doc()doc";

static const char* __doc_operations_research_SortedDisjointIntervalList_begin =
    R"doc(Const iterators for SortedDisjoinIntervalList.

One example usage is to use range loops in C++:
SortedDisjointIntervalList list; ... for (const ClosedInterval&
interval : list) { ... })doc";

static const char* __doc_operations_research_SortedDisjointIntervalList_clear =
    R"doc()doc";

static const char* __doc_operations_research_SortedDisjointIntervalList_end =
    R"doc()doc";

static const char*
    __doc_operations_research_SortedDisjointIntervalList_intervals =
        R"doc()doc";

static const char* __doc_operations_research_SortedDisjointIntervalList_last =
    R"doc(Returns a const& to the last interval. The list must not be empty.)doc";

static const char* __doc_operations_research_SortedDisjointIntervalList_swap =
    R"doc()doc";

static const char* __doc_operations_research_SumOfKMaxValueInDomain =
    R"doc()doc";

static const char* __doc_operations_research_SumOfKMinValueInDomain =
    R"doc()doc";

static const char* __doc_operations_research_operator_lshift = R"doc()doc";

static const char* __doc_operations_research_operator_lshift_2 = R"doc()doc";

static const char* __doc_operations_research_operator_lshift_3 = R"doc()doc";

#if defined(__GNUG__)
#pragma GCC diagnostic pop
#endif

#endif  // OR_TOOLS_UTIL_PYTHON_SORTED_INTERVAL_LIST_DOC_H_
