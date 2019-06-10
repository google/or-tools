---
title: 'OR-Tools'
---

Class List {#_annotated}
==========

Here are the classes, structs, unions and interfaces with brief
descriptions:

operations\_research::sat::AutomatonConstraint

operations\_research::sat::BoolVar

operations\_research::sat::CircuitConstraint

operations\_research::ClosedInterval

Represents a closed interval \[start, end\]. We must have start \<= end

operations\_research::sat::Constraint

operations\_research::sat::CpModelBuilder

operations\_research::sat::CumulativeConstraint

operations\_research::Domain

operations\_research::SortedDisjointIntervalList::IntervalComparator

operations\_research::sat::IntervalVar

operations\_research::sat::IntVar

operations\_research::sat::LinearExpr

operations\_research::sat::Model

operations\_research::sat::NoOverlap2DConstraint

operations\_research::sat::ReservoirConstraint

operations\_research::SortedDisjointIntervalList

operations\_research::sat::TableConstraint

operations\_research::Domain Class Reference {#_classoperations__research_1_1Domain}
============================================

operations\_research::Domain

`#include <sorted_interval_list.h>`

-   [Domain](#_classoperations__research_1_1Domain_1a8026c98d5255e56fc3f224be687a07c3)
    ()

    By default, [Domain](#_classoperations__research_1_1Domain) will be
    empty.

-   [Domain](#_classoperations__research_1_1Domain_1a4604d54b843d603bde4f76ef853464e6)
    (int64 value)

    Constructor for the common case of a singleton domain.

-   [Domain](#_classoperations__research_1_1Domain_1a8c763d670dcbb2656b8b708bf50d7262)
    (int64 left, int64 right)

-   std::vector\< int64 \>
    [FlattenedIntervals](#_classoperations__research_1_1Domain_1a2de0f6b61253050ad242107ce42ba825)
    () const

-   bool
    [IsEmpty](#_classoperations__research_1_1Domain_1a7f13c8b45290e4cf8889c4e677b0cd85)
    () const

    Returns true if this is the empty set.

-   int64
    [Size](#_classoperations__research_1_1Domain_1afc9e0537e74ddbb050a001b96f4ca05c)
    () const

    Returns the number of elements in the domain. It is capped at
    kint64max.

-   int64
    [Min](#_classoperations__research_1_1Domain_1a9ed997be60938397604bc2761b0c6775)
    () const

-   int64
    [Max](#_classoperations__research_1_1Domain_1a23a06dbf9d08cc91d8de1fe86b8bccc9)
    () const

-   bool
    [Contains](#_classoperations__research_1_1Domain_1aca55d3d10ee99aeab77e457d216d8c02)
    (int64 value) const

    Returns true iff value is in
    [Domain](#_classoperations__research_1_1Domain).

-   bool
    [IsIncludedIn](#_classoperations__research_1_1Domain_1a770758f5d9d0569a04c3e203cb2ce216)
    (const [Domain](#_classoperations__research_1_1Domain) &domain)
    const

    Returns true iff D is included in the given domain.

-   [Domain](#_classoperations__research_1_1Domain)
    [Complement](#_classoperations__research_1_1Domain_1ac6beb8d0bb66ee165d94623d5a704abd)
    () const

    Returns the set Int64 ∖ D.

-   [Domain](#_classoperations__research_1_1Domain)
    [Negation](#_classoperations__research_1_1Domain_1a4b24ac9e300406590558a82ae378db1e)
    () const

-   [Domain](#_classoperations__research_1_1Domain)
    [IntersectionWith](#_classoperations__research_1_1Domain_1a4bf09b90ae38afd5bd3aa87ed5e0dc04)
    (const [Domain](#_classoperations__research_1_1Domain) &domain)
    const

    Returns the set D ∩ domain.

-   [Domain](#_classoperations__research_1_1Domain)
    [UnionWith](#_classoperations__research_1_1Domain_1ac33030581b21c1e9fc6ec13a0486c28e)
    (const [Domain](#_classoperations__research_1_1Domain) &domain)
    const

    Returns the set D ∪ domain.

-   [Domain](#_classoperations__research_1_1Domain)
    [AdditionWith](#_classoperations__research_1_1Domain_1a714a1473bb78dab3195bd5cd5e90af42)
    (const [Domain](#_classoperations__research_1_1Domain) &domain)
    const

    Returns {x ∈ Int64, ∃ a ∈ D, ∃ b ∈ domain, x = a + b}.

-   [Domain](#_classoperations__research_1_1Domain)
    [MultiplicationBy](#_classoperations__research_1_1Domain_1a0d8ff8a937591b31265b86db6353fa34)
    (int64 coeff, bool \*exact=nullptr) const

-   [Domain](#_classoperations__research_1_1Domain)
    [RelaxIfTooComplex](#_classoperations__research_1_1Domain_1ab06560137156458393e8f44acfd01712)
    () const

    If
    [NumIntervals()](#_classoperations__research_1_1Domain_1a7c440e0dfafdb9c1299576f17a52b34e)
    is too large, this return a superset of the domain.

-   [Domain](#_classoperations__research_1_1Domain)
    [ContinuousMultiplicationBy](#_classoperations__research_1_1Domain_1a5fa203b31737f4c452a0b04779ce45a8)
    (int64 coeff) const

-   [Domain](#_classoperations__research_1_1Domain)
    [ContinuousMultiplicationBy](#_classoperations__research_1_1Domain_1a4c1fd36870e1dbf2029c462d3fb3d517)
    (const [Domain](#_classoperations__research_1_1Domain) &domain)
    const

-   [Domain](#_classoperations__research_1_1Domain)
    [DivisionBy](#_classoperations__research_1_1Domain_1a9f653103edc20fa77ecdd7708c76d037)
    (int64 coeff) const

-   [Domain](#_classoperations__research_1_1Domain)
    [InverseMultiplicationBy](#_classoperations__research_1_1Domain_1a3b6727dfefc8413b855926e1c4d27da6)
    (const int64 coeff) const

-   [Domain](#_classoperations__research_1_1Domain)
    [SimplifyUsingImpliedDomain](#_classoperations__research_1_1Domain_1a0217b50fb7a5dc20f697ef3d0b14ec41)
    (const [Domain](#_classoperations__research_1_1Domain)
    &implied\_domain) const

-   std::string
    [ToString](#_classoperations__research_1_1Domain_1a94b8d91180431ec7bf1073c2a8538f70)
    () const

-   bool
    [operator\<](#_classoperations__research_1_1Domain_1a27c69d2928356d00ee0624323e116fc8)
    (const [Domain](#_classoperations__research_1_1Domain) &other) const

    Lexicographic order on the
    [intervals()](#_classoperations__research_1_1Domain_1a153837b45a0e9ddf5620679f243baa96)
    representation.

-   bool
    [operator==](#_classoperations__research_1_1Domain_1acf15b6795a380eeb7521e1a8e15ee5d9)
    (const [Domain](#_classoperations__research_1_1Domain) &other) const

-   bool
    [operator!=](#_classoperations__research_1_1Domain_1afc2fcedb7011ed72d191ca8be79e2ec7)
    (const [Domain](#_classoperations__research_1_1Domain) &other) const

-   int
    [NumIntervals](#_classoperations__research_1_1Domain_1a7c440e0dfafdb9c1299576f17a52b34e)
    () const

-   [ClosedInterval](#_structoperations__research_1_1ClosedInterval)
    [front](#_classoperations__research_1_1Domain_1aede4d7f8e8486355c2eb4d9604ed20db)
    () const

-   [ClosedInterval](#_structoperations__research_1_1ClosedInterval)
    [back](#_classoperations__research_1_1Domain_1ac61a6df7b21becb7105b149b35992fbb)
    () const

-   [ClosedInterval](#_structoperations__research_1_1ClosedInterval)
    [operator\[\]](#_classoperations__research_1_1Domain_1ad3bfaa1ea6a1fce5a0c5a8ee61bfca3f)
    (int i) const

-   absl::InlinedVector\<
    [ClosedInterval](#_structoperations__research_1_1ClosedInterval),
    1 \>::const\_iterator
    [begin](#_classoperations__research_1_1Domain_1abae404f5f46ad1b7cd495000aec19639)
    () const

-   absl::InlinedVector\<
    [ClosedInterval](#_structoperations__research_1_1ClosedInterval),
    1 \>::const\_iterator
    [end](#_classoperations__research_1_1Domain_1a5c5e5505773dda5ca11ff5fddd70ae9b)
    () const

-   std::vector\<
    [ClosedInterval](#_structoperations__research_1_1ClosedInterval) \>
    [intervals](#_classoperations__research_1_1Domain_1a153837b45a0e9ddf5620679f243baa96)
    () const

<!-- -->

-   static [Domain](#_classoperations__research_1_1Domain)
    [AllValues](#_classoperations__research_1_1Domain_1a5d6343e6f8f0356f0270b25e76aa03b2)
    ()

    Returns the full domain Int64.

-   static [Domain](#_classoperations__research_1_1Domain)
    [FromValues](#_classoperations__research_1_1Domain_1afb43df1420e70ff6feda3557a1142dfc)
    (std::vector\< int64 \> values)

-   static [Domain](#_classoperations__research_1_1Domain)
    [FromIntervals](#_classoperations__research_1_1Domain_1a3f44e38ea10baff35e9d8f210e5900ed)
    (absl::Span\< const
    [ClosedInterval](#_structoperations__research_1_1ClosedInterval) \>
    [intervals](#_classoperations__research_1_1Domain_1a153837b45a0e9ddf5620679f243baa96))

    Creates a domain from the union of an unsorted list of intervals.

-   static [Domain](#_classoperations__research_1_1Domain)
    [FromVectorIntervals](#_classoperations__research_1_1Domain_1a5840372e81d2500f8aeb77d880d22bc6)
    (const std::vector\< std::vector\< int64 \> \>
    &[intervals](#_classoperations__research_1_1Domain_1a153837b45a0e9ddf5620679f243baa96))

    Used in non-C++ languages. Do not use directly.

-   static [Domain](#_classoperations__research_1_1Domain)
    [FromFlatIntervals](#_classoperations__research_1_1Domain_1a130878d13a4dd79365b1ecfa171b4714)
    (const std::vector\< int64 \> &flat\_intervals)

Detailed Description
--------------------

We call \"domain\" any subset of Int64 = \[kint64min, kint64max\].

This class can be used to represent such set efficiently as a sorted and
non-adjacent list of intervals. This is efficient as long as the size of
such list stays reasonable.

In the comments below, the domain of \*this will always be written
\'D\'.

> **Note**
>
> all the functions are safe with respect to integer overflow.

Constructor & Destructor Documentation
--------------------------------------

### Domain()`[1/3]`

Domain

operations\_research::Domain

operations\_research::Domain

Domain

`operations_research::Domain::Domain ( )[inline]`

By default, [Domain](#_classoperations__research_1_1Domain) will be
empty.

### Domain()`[2/3]`

Domain

operations\_research::Domain

operations\_research::Domain

Domain

`operations_research::Domain::Domain (int64 value)[explicit]`

Constructor for the common case of a singleton domain.

### Domain()`[3/3]`

Domain

operations\_research::Domain

operations\_research::Domain

Domain

`operations_research::Domain::Domain (int64 left, int64 right)`

Constructor for the common case of a single interval \[left, right\]. If
left \> right, this will result in the empty domain.

Member Function Documentation
-----------------------------

### AdditionWith()

AdditionWith

operations\_research::Domain

operations\_research::Domain

AdditionWith

`Domain operations_research::Domain::AdditionWith (const Domain & domain) const`

Returns {x ∈ Int64, ∃ a ∈ D, ∃ b ∈ domain, x = a + b}.

### AllValues()

AllValues

operations\_research::Domain

operations\_research::Domain

AllValues

`static Domain operations_research::Domain::AllValues ( )[static]`

Returns the full domain Int64.

### back()

back

operations\_research::Domain

operations\_research::Domain

back

`ClosedInterval operations_research::Domain::back ( ) const[inline]`

### begin()

begin

operations\_research::Domain

operations\_research::Domain

begin

`absl::InlinedVector<ClosedInterval, 1>::const_iterator operations_research::Domain::begin ( ) const[inline]`

### Complement()

Complement

operations\_research::Domain

operations\_research::Domain

Complement

`Domain operations_research::Domain::Complement ( ) const`

Returns the set Int64 ∖ D.

### Contains()

Contains

operations\_research::Domain

operations\_research::Domain

Contains

`bool operations_research::Domain::Contains (int64 value) const`

Returns true iff value is in
[Domain](#_classoperations__research_1_1Domain).

### ContinuousMultiplicationBy()`[1/2]`

ContinuousMultiplicationBy

operations\_research::Domain

operations\_research::Domain

ContinuousMultiplicationBy

`Domain operations_research::Domain::ContinuousMultiplicationBy (int64 coeff) const`

Returns a super-set of
[MultiplicationBy()](#_classoperations__research_1_1Domain_1a0d8ff8a937591b31265b86db6353fa34)
to avoid the explosion in the representation size. This behaves as if we
replace the set D of non-adjacent integer intervals by the set of
floating-point element in the same intervals.

For instance, \[1, 100\] \* 2 will be transformed in \[2, 200\] and not
in \[2\]\[4\]\[6\]\...\[200\] like in
[MultiplicationBy()](#_classoperations__research_1_1Domain_1a0d8ff8a937591b31265b86db6353fa34).
Note that this would be similar to a InverseDivisionBy(), but not quite
the same because if we look for {x ∈ Int64, ∃ e ∈ D, x / coeff = e},
then we will get \[2, 201\] in the case above.

### ContinuousMultiplicationBy()`[2/2]`

ContinuousMultiplicationBy

operations\_research::Domain

operations\_research::Domain

ContinuousMultiplicationBy

`Domain operations_research::Domain::ContinuousMultiplicationBy (const Domain & domain) const`

### DivisionBy()

DivisionBy

operations\_research::Domain

operations\_research::Domain

DivisionBy

`Domain operations_research::Domain::DivisionBy (int64 coeff) const`

Returns {x ∈ Int64, ∃ e ∈ D, x = e / coeff}.

For instance Domain(1, 7).DivisionBy(2) == Domain(0, 3).

### end()

end

operations\_research::Domain

operations\_research::Domain

end

`absl::InlinedVector<ClosedInterval, 1>::const_iterator operations_research::Domain::end ( ) const[inline]`

### FlattenedIntervals()

FlattenedIntervals

operations\_research::Domain

operations\_research::Domain

FlattenedIntervals

`std::vector<int64> operations_research::Domain::FlattenedIntervals ( ) const`

### FromFlatIntervals()

FromFlatIntervals

operations\_research::Domain

operations\_research::Domain

FromFlatIntervals

`static Domain operations_research::Domain::FromFlatIntervals (const std::vector< int64 > & flat_intervals)[static]`

### FromIntervals()

FromIntervals

operations\_research::Domain

operations\_research::Domain

FromIntervals

`static Domain operations_research::Domain::FromIntervals (absl::Span< const ClosedInterval > intervals)[static]`

Creates a domain from the union of an unsorted list of intervals.

### FromValues()

FromValues

operations\_research::Domain

operations\_research::Domain

FromValues

`static Domain operations_research::Domain::FromValues (std::vector< int64 > values)[static]`

Creates a domain from the union of an unsorted list of integer values.
Input values may be repeated, with no consequence on the output

### FromVectorIntervals()

FromVectorIntervals

operations\_research::Domain

operations\_research::Domain

FromVectorIntervals

`static Domain operations_research::Domain::FromVectorIntervals (const std::vector< std::vector< int64 > > & intervals)[static]`

Used in non-C++ languages. Do not use directly.

### front()

front

operations\_research::Domain

operations\_research::Domain

front

`ClosedInterval operations_research::Domain::front ( ) const[inline]`

### IntersectionWith()

IntersectionWith

operations\_research::Domain

operations\_research::Domain

IntersectionWith

`Domain operations_research::Domain::IntersectionWith (const Domain & domain) const`

Returns the set D ∩ domain.

### intervals()

intervals

operations\_research::Domain

operations\_research::Domain

intervals

`std::vector<ClosedInterval> operations_research::Domain::intervals ( ) const[inline]`

[Deprecated](#_deprecated_1_deprecated000001)

[Todo](#_todo_1_todo000006)

(user): remove, this makes a copy and is of a different type that our
internal InlinedVector() anyway.

### InverseMultiplicationBy()

InverseMultiplicationBy

operations\_research::Domain

operations\_research::Domain

InverseMultiplicationBy

`Domain operations_research::Domain::InverseMultiplicationBy (const int64 coeff) const`

Returns {x ∈ Int64, ∃ e ∈ D, x \* coeff = e}.

For instance Domain(1, 7).InverseMultiplicationBy(2) == Domain(1, 3).

### IsEmpty()

IsEmpty

operations\_research::Domain

operations\_research::Domain

IsEmpty

`bool operations_research::Domain::IsEmpty ( ) const`

Returns true if this is the empty set.

### IsIncludedIn()

IsIncludedIn

operations\_research::Domain

operations\_research::Domain

IsIncludedIn

`bool operations_research::Domain::IsIncludedIn (const Domain & domain) const`

Returns true iff D is included in the given domain.

### Max()

Max

operations\_research::Domain

operations\_research::Domain

Max

`int64 operations_research::Domain::Max ( ) const`

### Min()

Min

operations\_research::Domain

operations\_research::Domain

Min

`int64 operations_research::Domain::Min ( ) const`

Returns the domain min/max value. This Checks that the domain is not
empty.

### MultiplicationBy()

MultiplicationBy

operations\_research::Domain

operations\_research::Domain

MultiplicationBy

`Domain operations_research::Domain::MultiplicationBy (int64 coeff, bool * exact = nullptr
) const`

Returns {x ∈ Int64, ∃ e ∈ D, x = e \* coeff}.

> **Note**
>
> because the resulting domain will only contains multiple of coeff, the
> size of intervals.size() can become really large. If it is larger than
> a fixed constant, exact will be set to false and the result will be
> set to ContinuousMultiplicationBy(coeff).

### Negation()

Negation

operations\_research::Domain

operations\_research::Domain

Negation

`Domain operations_research::Domain::Negation ( ) const`

Returns {x ∈ Int64, ∃ e ∈ D, x = -e}.

Note in particular that if the negation of Int64 is not Int64 but Int64
\\ {kint64min} !!

### NumIntervals()

NumIntervals

operations\_research::Domain

operations\_research::Domain

NumIntervals

`int operations_research::Domain::NumIntervals ( ) const[inline]`

Basic read-only std::vector\<\> wrapping to view a
[Domain](#_classoperations__research_1_1Domain) as a sorted list of
non-adjacent intervals. Note that we don\'t expose size() which might be
confused with the number of values in the domain.

### operator!=()

operator!=

operations\_research::Domain

operations\_research::Domain

operator!=

`bool operations_research::Domain::operator!= (const Domain & other) const[inline]`

### operator\<()

operator\<

operations\_research::Domain

operations\_research::Domain

operator\<

`bool operations_research::Domain::operator< (const Domain & other) const`

Lexicographic order on the
[intervals()](#_classoperations__research_1_1Domain_1a153837b45a0e9ddf5620679f243baa96)
representation.

### operator==()

operator==

operations\_research::Domain

operations\_research::Domain

operator==

`bool operations_research::Domain::operator== (const Domain & other) const[inline]`

### operator\[\]()

operator\[\]

operations\_research::Domain

operations\_research::Domain

operator\[\]

`ClosedInterval operations_research::Domain::operator[] (int i) const[inline]`

### RelaxIfTooComplex()

RelaxIfTooComplex

operations\_research::Domain

operations\_research::Domain

RelaxIfTooComplex

`Domain operations_research::Domain::RelaxIfTooComplex ( ) const`

If
[NumIntervals()](#_classoperations__research_1_1Domain_1a7c440e0dfafdb9c1299576f17a52b34e)
is too large, this return a superset of the domain.

### SimplifyUsingImpliedDomain()

SimplifyUsingImpliedDomain

operations\_research::Domain

operations\_research::Domain

SimplifyUsingImpliedDomain

`Domain operations_research::Domain::SimplifyUsingImpliedDomain (const Domain & implied_domain) const`

Advanced usage. Given some \"implied\" information on this domain that
is assumed to be always true (i.e. only values in the intersection with
implied domain matter), this function will simplify the current domain
without changing the set of \"possible values\".

More precisely, this will:

-   Take the intersection with implied\_domain.

-   Minimize the number of intervals. That is, if the domain is like
    \[1,2\]\[4\] and implied is \[1\]\[4\], then the domain can be
    relaxed to \[1, 4\] to simplify its complexity without changing the
    set of admissible value assuming only implied values can be seen.

-   Restrict as much as possible the bounds of the remaining intervals.
    I.e if the input is \[1,2\] and implied is \[0,4\], then the domain
    will not be changed.

> **Note**
>
> domain.SimplifyUsingImpliedDomain(domain) will just return
> \[domain.Min(), domain.Max()\]. This is meant to be applied to the rhs
> of a constraint to make its propagation more efficient.

### Size()

Size

operations\_research::Domain

operations\_research::Domain

Size

`int64 operations_research::Domain::Size ( ) const`

Returns the number of elements in the domain. It is capped at kint64max.

### ToString()

ToString

operations\_research::Domain

operations\_research::Domain

ToString

`std::string operations_research::Domain::ToString ( ) const`

Returns a compact std::string of a vector of intervals like
\"\[1,4\]\[6\]\[10,20\]\"

### UnionWith()

UnionWith

operations\_research::Domain

operations\_research::Domain

UnionWith

`Domain operations_research::Domain::UnionWith (const Domain & domain) const`

Returns the set D ∪ domain.

The documentation for this class was generated from the following file:

ortools/util/

sorted\_interval\_list.h

operations\_research::SortedDisjointIntervalList Class Reference {#_classoperations__research_1_1SortedDisjointIntervalList}
================================================================

operations\_research::SortedDisjointIntervalList

`#include <sorted_interval_list.h>`

-   struct
    [IntervalComparator](#_structoperations__research_1_1SortedDisjointIntervalList_1_1IntervalComparator)

<!-- -->

-   typedef std::set\<
    [ClosedInterval](#_structoperations__research_1_1ClosedInterval),
    [IntervalComparator](#_structoperations__research_1_1SortedDisjointIntervalList_1_1IntervalComparator) \>
    [IntervalSet](#_classoperations__research_1_1SortedDisjointIntervalList_1aa2245d981f2499b2948864f9e717b638)

-   typedef IntervalSet::iterator
    [Iterator](#_classoperations__research_1_1SortedDisjointIntervalList_1aad3f138b3bec53adabe26c6d8a3f8b4b)

<!-- -->

-   [SortedDisjointIntervalList](#_classoperations__research_1_1SortedDisjointIntervalList_1a1d800ef9b7bcda7b2bd88941e63e9c0d)
    ()

-   [SortedDisjointIntervalList](#_classoperations__research_1_1SortedDisjointIntervalList_1a1112195975448318ee0d3a0ca17d8077)
    (const std::vector\<
    [ClosedInterval](#_structoperations__research_1_1ClosedInterval) \>
    &intervals)

-   [SortedDisjointIntervalList](#_classoperations__research_1_1SortedDisjointIntervalList_1a85b8188b348ebff82e5ff38ef49da201)
    (const std::vector\< int64 \> &starts, const std::vector\< int64 \>
    &ends)

-   [SortedDisjointIntervalList](#_classoperations__research_1_1SortedDisjointIntervalList_1ac2174bbccd9beafcfc3e3009264b968f)
    (const std::vector\< int \> &starts, const std::vector\< int \>
    &ends)

-   [SortedDisjointIntervalList](#_classoperations__research_1_1SortedDisjointIntervalList)
    [BuildComplementOnInterval](#_classoperations__research_1_1SortedDisjointIntervalList_1ac13367be885072fd7068f25fc9b726af)
    (int64 start, int64
    [end](#_classoperations__research_1_1SortedDisjointIntervalList_1ae7c2e3e4d3cddf60fd1e5340a956e716))

    Builds the complement of the interval list on the interval \[start,
    end\].

-   [Iterator](#_classoperations__research_1_1SortedDisjointIntervalList_1aad3f138b3bec53adabe26c6d8a3f8b4b)
    [InsertInterval](#_classoperations__research_1_1SortedDisjointIntervalList_1a3ff158f563c55210b76f860b30274daf)
    (int64 start, int64
    [end](#_classoperations__research_1_1SortedDisjointIntervalList_1ae7c2e3e4d3cddf60fd1e5340a956e716))

-   [Iterator](#_classoperations__research_1_1SortedDisjointIntervalList_1aad3f138b3bec53adabe26c6d8a3f8b4b)
    [GrowRightByOne](#_classoperations__research_1_1SortedDisjointIntervalList_1aede8a3e747198ce6223d9f2ed788a3b3)
    (int64 value, int64 \*newly\_covered)

-   void
    [InsertIntervals](#_classoperations__research_1_1SortedDisjointIntervalList_1ab2f22ce88ed4581b3e4859a2e4504e9e)
    (const std::vector\< int64 \> &starts, const std::vector\< int64 \>
    &ends)

-   void
    [InsertIntervals](#_classoperations__research_1_1SortedDisjointIntervalList_1ad937d5864aa7a209f5c565a49daad904)
    (const std::vector\< int \> &starts, const std::vector\< int \>
    &ends)

-   int
    [NumIntervals](#_classoperations__research_1_1SortedDisjointIntervalList_1a6d5b1054cf1c7338bfd929bf2460140c)
    () const

    Returns the number of disjoint intervals in the list.

-   [Iterator](#_classoperations__research_1_1SortedDisjointIntervalList_1aad3f138b3bec53adabe26c6d8a3f8b4b)
    [FirstIntervalGreaterOrEqual](#_classoperations__research_1_1SortedDisjointIntervalList_1a32e16ab60594417366e26339a1db03f3)
    (int64 value) const

-   [Iterator](#_classoperations__research_1_1SortedDisjointIntervalList_1aad3f138b3bec53adabe26c6d8a3f8b4b)
    [LastIntervalLessOrEqual](#_classoperations__research_1_1SortedDisjointIntervalList_1a0b57e677ac61bf5768d95bc64c0a4e4a)
    (int64 value) const

-   std::string
    [DebugString](#_classoperations__research_1_1SortedDisjointIntervalList_1a403ba2c2147b5f9086ca988671dd00fd)
    () const

-   const
    [Iterator](#_classoperations__research_1_1SortedDisjointIntervalList_1aad3f138b3bec53adabe26c6d8a3f8b4b)
    [begin](#_classoperations__research_1_1SortedDisjointIntervalList_1aa0c89218fd155f0cbe89cbfa05e35afc)
    () const

-   const
    [Iterator](#_classoperations__research_1_1SortedDisjointIntervalList_1aad3f138b3bec53adabe26c6d8a3f8b4b)
    [end](#_classoperations__research_1_1SortedDisjointIntervalList_1ae7c2e3e4d3cddf60fd1e5340a956e716)
    () const

-   const
    [ClosedInterval](#_structoperations__research_1_1ClosedInterval) &
    [last](#_classoperations__research_1_1SortedDisjointIntervalList_1a25eab92a71b4bb1fa45e9ce37c9b51f4)
    () const

    Returns a const& to the last interval. The list must not be empty.

-   void
    [clear](#_classoperations__research_1_1SortedDisjointIntervalList_1ab770345669a4b37c7df83dee5ebceb04)
    ()

-   void
    [swap](#_classoperations__research_1_1SortedDisjointIntervalList_1a92df806fd079d25dbff73b57d36485f0)
    ([SortedDisjointIntervalList](#_classoperations__research_1_1SortedDisjointIntervalList)
    &other)

Detailed Description
--------------------

This class represents a sorted list of disjoint, closed intervals. When
an interval is inserted, all intervals that overlap it or that are even
adjacent to it are merged into one. I.e. \[0,14\] and \[15,30\] will be
merged to \[0,30\].

Iterators returned by this class are invalidated by non-const
operations.

[Todo](#_todo_1_todo000005)

(user): Templatize the class on the type of the bounds.

Member Typedef Documentation
----------------------------

### IntervalSet

IntervalSet

operations\_research::SortedDisjointIntervalList

operations\_research::SortedDisjointIntervalList

IntervalSet

`typedef std::set<ClosedInterval, IntervalComparator> operations_research::SortedDisjointIntervalList::IntervalSet`

### Iterator

Iterator

operations\_research::SortedDisjointIntervalList

operations\_research::SortedDisjointIntervalList

Iterator

`typedef IntervalSet::iterator operations_research::SortedDisjointIntervalList::Iterator`

Constructor & Destructor Documentation
--------------------------------------

### SortedDisjointIntervalList()`[1/4]`

SortedDisjointIntervalList

operations\_research::SortedDisjointIntervalList

operations\_research::SortedDisjointIntervalList

SortedDisjointIntervalList

`operations_research::SortedDisjointIntervalList::SortedDisjointIntervalList ( )`

### SortedDisjointIntervalList()`[2/4]`

SortedDisjointIntervalList

operations\_research::SortedDisjointIntervalList

operations\_research::SortedDisjointIntervalList

SortedDisjointIntervalList

`operations_research::SortedDisjointIntervalList::SortedDisjointIntervalList (const std::vector< ClosedInterval > & intervals)[explicit]`

### SortedDisjointIntervalList()`[3/4]`

SortedDisjointIntervalList

operations\_research::SortedDisjointIntervalList

operations\_research::SortedDisjointIntervalList

SortedDisjointIntervalList

`operations_research::SortedDisjointIntervalList::SortedDisjointIntervalList (const std::vector< int64 > & starts, const std::vector< int64 > & ends)`

Creates a
[SortedDisjointIntervalList](#_classoperations__research_1_1SortedDisjointIntervalList)
and fills it with intervals \[starts\[i\]..ends\[i\]\]. All intervals
must be consistent (starts\[i\] \<= ends\[i\]). There\'s two version,
one for int64, one for int.

[Todo](#_todo_1_todo000007)

(user): Explain why we favored this API to the more natural input
std::vector\<ClosedInterval\> or std::vector\<std::pair\<int, int\>\>.

### SortedDisjointIntervalList()`[4/4]`

SortedDisjointIntervalList

operations\_research::SortedDisjointIntervalList

operations\_research::SortedDisjointIntervalList

SortedDisjointIntervalList

`operations_research::SortedDisjointIntervalList::SortedDisjointIntervalList (const std::vector< int > & starts, const std::vector< int > & ends)`

Member Function Documentation
-----------------------------

### begin()

begin

operations\_research::SortedDisjointIntervalList

operations\_research::SortedDisjointIntervalList

begin

`const Iterator operations_research::SortedDisjointIntervalList::begin ( ) const[inline]`

This is to use range loops in C++:
[SortedDisjointIntervalList](#_classoperations__research_1_1SortedDisjointIntervalList)
list; \... for (const
[ClosedInterval](#_structoperations__research_1_1ClosedInterval)
interval : list) { \... }

### BuildComplementOnInterval()

BuildComplementOnInterval

operations\_research::SortedDisjointIntervalList

operations\_research::SortedDisjointIntervalList

BuildComplementOnInterval

`SortedDisjointIntervalList operations_research::SortedDisjointIntervalList::BuildComplementOnInterval (int64 start, int64 end)`

Builds the complement of the interval list on the interval \[start,
end\].

### clear()

clear

operations\_research::SortedDisjointIntervalList

operations\_research::SortedDisjointIntervalList

clear

`void operations_research::SortedDisjointIntervalList::clear ( )[inline]`

### DebugString()

DebugString

operations\_research::SortedDisjointIntervalList

operations\_research::SortedDisjointIntervalList

DebugString

`std::string operations_research::SortedDisjointIntervalList::DebugString ( ) const`

### end()

end

operations\_research::SortedDisjointIntervalList

operations\_research::SortedDisjointIntervalList

end

`const Iterator operations_research::SortedDisjointIntervalList::end ( ) const[inline]`

### FirstIntervalGreaterOrEqual()

FirstIntervalGreaterOrEqual

operations\_research::SortedDisjointIntervalList

operations\_research::SortedDisjointIntervalList

FirstIntervalGreaterOrEqual

`Iterator operations_research::SortedDisjointIntervalList::FirstIntervalGreaterOrEqual (int64 value) const`

Returns an iterator to either:

-   the first interval containing or above the given value, or

-   the last interval containing or below the given value. Returns
    [end()](#_classoperations__research_1_1SortedDisjointIntervalList_1ae7c2e3e4d3cddf60fd1e5340a956e716)
    if no interval fulfils that condition.

If the value is within an interval, both functions will return it.

### GrowRightByOne()

GrowRightByOne

operations\_research::SortedDisjointIntervalList

operations\_research::SortedDisjointIntervalList

GrowRightByOne

`Iterator operations_research::SortedDisjointIntervalList::GrowRightByOne (int64 value, int64 * newly_covered)`

If value is in an interval, increase its end by one, otherwise insert
the interval \[value, value\]. In both cases, this returns an iterator
to the new/modified interval (possibly merged with others) and fills
newly\_covered with the new value that was just added in the union of
all the intervals.

If this causes an interval ending at kint64max to grow, it will die with
a CHECK fail.

### InsertInterval()

InsertInterval

operations\_research::SortedDisjointIntervalList

operations\_research::SortedDisjointIntervalList

InsertInterval

`Iterator operations_research::SortedDisjointIntervalList::InsertInterval (int64 start, int64 end)`

Adds the interval \[start..end\] to the list, and merges overlapping or
immediately adjacent intervals (\[2, 5\] and \[6, 7\] are adjacent, but
\[2, 5\] and \[7, 8\] are not).

Returns an iterator to the inserted interval (possibly merged with
others).

If start \> end, it does LOG(DFATAL) and returns
[end()](#_classoperations__research_1_1SortedDisjointIntervalList_1ae7c2e3e4d3cddf60fd1e5340a956e716)
(no interval added).

### InsertIntervals()`[1/2]`

InsertIntervals

operations\_research::SortedDisjointIntervalList

operations\_research::SortedDisjointIntervalList

InsertIntervals

`void operations_research::SortedDisjointIntervalList::InsertIntervals (const std::vector< int64 > & starts, const std::vector< int64 > & ends)`

Adds all intervals \[starts\[i\]..ends\[i\]\]. Same behavior as
[InsertInterval()](#_classoperations__research_1_1SortedDisjointIntervalList_1a3ff158f563c55210b76f860b30274daf)
upon invalid intervals. There\'s a version with int64 and int32.

### InsertIntervals()`[2/2]`

InsertIntervals

operations\_research::SortedDisjointIntervalList

operations\_research::SortedDisjointIntervalList

InsertIntervals

`void operations_research::SortedDisjointIntervalList::InsertIntervals (const std::vector< int > & starts, const std::vector< int > & ends)`

### last()

last

operations\_research::SortedDisjointIntervalList

operations\_research::SortedDisjointIntervalList

last

`const ClosedInterval& operations_research::SortedDisjointIntervalList::last ( ) const[inline]`

Returns a const& to the last interval. The list must not be empty.

### LastIntervalLessOrEqual()

LastIntervalLessOrEqual

operations\_research::SortedDisjointIntervalList

operations\_research::SortedDisjointIntervalList

LastIntervalLessOrEqual

`Iterator operations_research::SortedDisjointIntervalList::LastIntervalLessOrEqual (int64 value) const`

### NumIntervals()

NumIntervals

operations\_research::SortedDisjointIntervalList

operations\_research::SortedDisjointIntervalList

NumIntervals

`int operations_research::SortedDisjointIntervalList::NumIntervals ( ) const[inline]`

Returns the number of disjoint intervals in the list.

### swap()

swap

operations\_research::SortedDisjointIntervalList

operations\_research::SortedDisjointIntervalList

swap

`void operations_research::SortedDisjointIntervalList::swap (SortedDisjointIntervalList & other)[inline]`

The documentation for this class was generated from the following file:

ortools/util/

sorted\_interval\_list.h

operations\_research::sat::AutomatonConstraint Class Reference {#_classoperations__research_1_1sat_1_1AutomatonConstraint}
==============================================================

operations\_research::sat::AutomatonConstraint

`#include <cp_model.h>`

Inheritance diagram for operations\_research::sat::AutomatonConstraint:

Collaboration diagram for
operations\_research::sat::AutomatonConstraint:

-   void
    [AddTransition](#_classoperations__research_1_1sat_1_1AutomatonConstraint_1a4fa8634eeba27c91397c58105ff50eb7)
    (int tail, int head, int64 transition\_label)

<!-- -->

-   class
    [CpModelBuilder](#_classoperations__research_1_1sat_1_1AutomatonConstraint_1ae04c85577cf33a05fb50bb361877fb42)

Detailed Description
--------------------

Specialized automaton constraint.

This constraint allows adding transitions to the automaton constraint
incrementally.

Member Function Documentation
-----------------------------

### AddTransition()

AddTransition

operations\_research::sat::AutomatonConstraint

operations\_research::sat::AutomatonConstraint

AddTransition

`void operations_research::sat::AutomatonConstraint::AddTransition (int tail, int head, int64 transition_label)`

Friends And Related Function Documentation
------------------------------------------

### CpModelBuilder

CpModelBuilder

operations\_research::sat::AutomatonConstraint

operations\_research::sat::AutomatonConstraint

CpModelBuilder

`friend class CpModelBuilder[friend]`

The documentation for this class was generated from the following file:

ortools/sat/

cp\_model.h

operations\_research::sat::BoolVar Class Reference {#_classoperations__research_1_1sat_1_1BoolVar}
==================================================

operations\_research::sat::BoolVar

`#include <cp_model.h>`

-   [BoolVar](#_classoperations__research_1_1sat_1_1BoolVar_1a8467b4b5dffef99ffb96ef6b9b4a4097)
    ()

-   [BoolVar](#_classoperations__research_1_1sat_1_1BoolVar)
    [WithName](#_classoperations__research_1_1sat_1_1BoolVar_1a1963637fcd9bfe8f9bd85a0971c0270d)
    (const std::string &name)

    Sets the name of the variable.

-   const std::string &
    [Name](#_classoperations__research_1_1sat_1_1BoolVar_1abcebeff89abbdb6b0b812616f1517f25)
    () const

-   [BoolVar](#_classoperations__research_1_1sat_1_1BoolVar)
    [Not](#_classoperations__research_1_1sat_1_1BoolVar_1ac5a3346c2302559c71bd9cd1e989edf9)
    () const

    Returns the logical negation of the current Boolean variable.

-   bool
    [operator==](#_classoperations__research_1_1sat_1_1BoolVar_1afb7fdd0dab72ba28030fb6d03ce5c32f)
    (const [BoolVar](#_classoperations__research_1_1sat_1_1BoolVar)
    &other) const

-   bool
    [operator!=](#_classoperations__research_1_1sat_1_1BoolVar_1a6e9d4868f30b80fa5c37ac8991726110)
    (const [BoolVar](#_classoperations__research_1_1sat_1_1BoolVar)
    &other) const

-   std::string
    [DebugString](#_classoperations__research_1_1sat_1_1BoolVar_1afb03d8ed70e426b0f7b83c76fce3c68f)
    () const

-   const IntegerVariableProto &
    [Proto](#_classoperations__research_1_1sat_1_1BoolVar_1a379713d334c199eeb834c338385293ba)
    () const

    Useful for testing.

-   IntegerVariableProto \*
    [MutableProto](#_classoperations__research_1_1sat_1_1BoolVar_1ae7e96dfb8ae534a787632d78711f9a44)
    () const

    Useful for model edition.

-   int
    [index](#_classoperations__research_1_1sat_1_1BoolVar_1a27d52277902e0d08306697a43863b5e8)
    () const

<!-- -->

-   class
    [CircuitConstraint](#_classoperations__research_1_1sat_1_1BoolVar_1a946eae8a695dfad4799c1efecec379e6)

-   class
    [Constraint](#_classoperations__research_1_1sat_1_1BoolVar_1a697ed9eaa8955d595a023663ab1e8418)

-   class
    [CpModelBuilder](#_classoperations__research_1_1sat_1_1BoolVar_1ae04c85577cf33a05fb50bb361877fb42)

-   class
    [IntVar](#_classoperations__research_1_1sat_1_1BoolVar_1a34419e55556ff4e92b447fe895bdb9c3)

-   class
    [IntervalVar](#_classoperations__research_1_1sat_1_1BoolVar_1afc7f9983234a41167299a74f07ec6622)

-   class
    [LinearExpr](#_classoperations__research_1_1sat_1_1BoolVar_1a7678a938bf60a5c17fb47cf58995db0c)

-   class
    [ReservoirConstraint](#_classoperations__research_1_1sat_1_1BoolVar_1ae0ff478f6506cb705bbc1737598276f4)

-   bool
    [SolutionBooleanValue](#_classoperations__research_1_1sat_1_1BoolVar_1a8391a20c25890ccbf3f5e3982afed236)
    (const CpSolverResponse &r,
    [BoolVar](#_classoperations__research_1_1sat_1_1BoolVar) x)

Detailed Description
--------------------

A Boolean variable. This class wraps an IntegerVariableProto with domain
\[0, 1\]. It supports logical negation (Not).

This can only be constructed via
[CpModelBuilder.NewBoolVar()](#_classoperations__research_1_1sat_1_1CpModelBuilder_1a8fc4a0c717f985687d63a586dba04641).

Constructor & Destructor Documentation
--------------------------------------

### BoolVar()

BoolVar

operations\_research::sat::BoolVar

operations\_research::sat::BoolVar

BoolVar

`operations_research::sat::BoolVar::BoolVar ( )`

Member Function Documentation
-----------------------------

### DebugString()

DebugString

operations\_research::sat::BoolVar

operations\_research::sat::BoolVar

DebugString

`std::string operations_research::sat::BoolVar::DebugString ( ) const`

### index()

index

operations\_research::sat::BoolVar

operations\_research::sat::BoolVar

index

`int operations_research::sat::BoolVar::index ( ) const[inline]`

Returns the index of the variable in the model. If the variable is the
negation of another variable v, its index is -v.index() - 1.

### MutableProto()

MutableProto

operations\_research::sat::BoolVar

operations\_research::sat::BoolVar

MutableProto

`IntegerVariableProto* operations_research::sat::BoolVar::MutableProto ( ) const[inline]`

Useful for model edition.

### Name()

Name

operations\_research::sat::BoolVar

operations\_research::sat::BoolVar

Name

`const std::string& operations_research::sat::BoolVar::Name ( ) const[inline]`

### Not()

Not

operations\_research::sat::BoolVar

operations\_research::sat::BoolVar

Not

`BoolVar operations_research::sat::BoolVar::Not ( ) const[inline]`

Returns the logical negation of the current Boolean variable.

### operator!=()

operator!=

operations\_research::sat::BoolVar

operations\_research::sat::BoolVar

operator!=

`bool operations_research::sat::BoolVar::operator!= (const BoolVar & other) const[inline]`

### operator==()

operator==

operations\_research::sat::BoolVar

operations\_research::sat::BoolVar

operator==

`bool operations_research::sat::BoolVar::operator== (const BoolVar & other) const[inline]`

### Proto()

Proto

operations\_research::sat::BoolVar

operations\_research::sat::BoolVar

Proto

`const IntegerVariableProto& operations_research::sat::BoolVar::Proto ( ) const[inline]`

Useful for testing.

### WithName()

WithName

operations\_research::sat::BoolVar

operations\_research::sat::BoolVar

WithName

`BoolVar operations_research::sat::BoolVar::WithName (const std::string & name)`

Sets the name of the variable.

Friends And Related Function Documentation
------------------------------------------

### CircuitConstraint

CircuitConstraint

operations\_research::sat::BoolVar

operations\_research::sat::BoolVar

CircuitConstraint

`friend class CircuitConstraint[friend]`

### Constraint

Constraint

operations\_research::sat::BoolVar

operations\_research::sat::BoolVar

Constraint

`friend class Constraint[friend]`

### CpModelBuilder

CpModelBuilder

operations\_research::sat::BoolVar

operations\_research::sat::BoolVar

CpModelBuilder

`friend class CpModelBuilder[friend]`

### IntervalVar

IntervalVar

operations\_research::sat::BoolVar

operations\_research::sat::BoolVar

IntervalVar

`friend class IntervalVar[friend]`

### IntVar

IntVar

operations\_research::sat::BoolVar

operations\_research::sat::BoolVar

IntVar

`friend class IntVar[friend]`

### LinearExpr

LinearExpr

operations\_research::sat::BoolVar

operations\_research::sat::BoolVar

LinearExpr

`friend class LinearExpr[friend]`

### ReservoirConstraint

ReservoirConstraint

operations\_research::sat::BoolVar

operations\_research::sat::BoolVar

ReservoirConstraint

`friend class ReservoirConstraint[friend]`

### SolutionBooleanValue

SolutionBooleanValue

operations\_research::sat::BoolVar

operations\_research::sat::BoolVar

SolutionBooleanValue

`bool SolutionBooleanValue (const CpSolverResponse & r, BoolVar x)[friend]`

Returns the value of a Boolean literal (a Boolean variable or its
negation) in a solver response.

The documentation for this class was generated from the following file:

ortools/sat/

cp\_model.h

operations\_research::sat::CircuitConstraint Class Reference {#_classoperations__research_1_1sat_1_1CircuitConstraint}
============================================================

operations\_research::sat::CircuitConstraint

`#include <cp_model.h>`

Inheritance diagram for operations\_research::sat::CircuitConstraint:

Collaboration diagram for operations\_research::sat::CircuitConstraint:

-   void
    [AddArc](#_classoperations__research_1_1sat_1_1CircuitConstraint_1a9ee6aa474b9e4c2bcf8fab717079704d)
    (int tail, int head,
    [BoolVar](#_classoperations__research_1_1sat_1_1BoolVar) literal)

<!-- -->

-   class
    [CpModelBuilder](#_classoperations__research_1_1sat_1_1CircuitConstraint_1ae04c85577cf33a05fb50bb361877fb42)

Detailed Description
--------------------

Specialized circuit constraint.

This constraint allows adding arcs to the circuit constraint
incrementally.

Member Function Documentation
-----------------------------

### AddArc()

AddArc

operations\_research::sat::CircuitConstraint

operations\_research::sat::CircuitConstraint

AddArc

`void operations_research::sat::CircuitConstraint::AddArc (int tail, int head, BoolVar literal)`

Friends And Related Function Documentation
------------------------------------------

### CpModelBuilder

CpModelBuilder

operations\_research::sat::CircuitConstraint

operations\_research::sat::CircuitConstraint

CpModelBuilder

`friend class CpModelBuilder[friend]`

The documentation for this class was generated from the following file:

ortools/sat/

cp\_model.h

operations\_research::sat::Constraint Class Reference {#_classoperations__research_1_1sat_1_1Constraint}
=====================================================

operations\_research::sat::Constraint

`#include <cp_model.h>`

Inheritance diagram for operations\_research::sat::Constraint:

-   [Constraint](#_classoperations__research_1_1sat_1_1Constraint)
    [OnlyEnforceIf](#_classoperations__research_1_1sat_1_1Constraint_1a9052e9e1dd8248433909b5542f314add)
    (absl::Span\< const
    [BoolVar](#_classoperations__research_1_1sat_1_1BoolVar) \>
    literals)

-   [Constraint](#_classoperations__research_1_1sat_1_1Constraint)
    [OnlyEnforceIf](#_classoperations__research_1_1sat_1_1Constraint_1ab6457905c9a8cd1c5f92738d57e6f298)
    ([BoolVar](#_classoperations__research_1_1sat_1_1BoolVar) literal)

    See [OnlyEnforceIf(absl::Span\<const BoolVar\>
    literals)](#_classoperations__research_1_1sat_1_1Constraint_1a9052e9e1dd8248433909b5542f314add).

-   [Constraint](#_classoperations__research_1_1sat_1_1Constraint)
    [WithName](#_classoperations__research_1_1sat_1_1Constraint_1a9401ab195650160402df5b61f8ac9bda)
    (const std::string &name)

    Sets the name of the constraint.

-   const std::string &
    [Name](#_classoperations__research_1_1sat_1_1Constraint_1aeaf30f4ee7d141e68905f1ac2432b937)
    () const

-   const ConstraintProto &
    [Proto](#_classoperations__research_1_1sat_1_1Constraint_1aa0b277df64333f670b66c8d5295b8250)
    () const

    Useful for testing.

-   ConstraintProto \*
    [MutableProto](#_classoperations__research_1_1sat_1_1Constraint_1acaa17b2fbfd62f6845329ae944835654)
    () const

    Useful for model edition.

<!-- -->

-   [Constraint](#_classoperations__research_1_1sat_1_1Constraint_1a9a6b9b664d4d0d56e8c0d14c8ad3bad9)
    (ConstraintProto \*proto)

<!-- -->

-   ConstraintProto \*
    [proto\_](#_classoperations__research_1_1sat_1_1Constraint_1a9d74c3d77f601020ab87700745f830ad)

<!-- -->

-   class
    [CpModelBuilder](#_classoperations__research_1_1sat_1_1Constraint_1ae04c85577cf33a05fb50bb361877fb42)

Detailed Description
--------------------

A constraint.

This class enable modifying the constraint that was added to the model.

It can anly be built by the different CpModelBuilder::AddXXX methods.

Constructor & Destructor Documentation
--------------------------------------

### Constraint()

Constraint

operations\_research::sat::Constraint

operations\_research::sat::Constraint

Constraint

`operations_research::sat::Constraint::Constraint (ConstraintProto * proto)[explicit], [protected]`

Member Function Documentation
-----------------------------

### MutableProto()

MutableProto

operations\_research::sat::Constraint

operations\_research::sat::Constraint

MutableProto

`ConstraintProto* operations_research::sat::Constraint::MutableProto ( ) const[inline]`

Useful for model edition.

### Name()

Name

operations\_research::sat::Constraint

operations\_research::sat::Constraint

Name

`const std::string& operations_research::sat::Constraint::Name ( ) const`

### OnlyEnforceIf()`[1/2]`

OnlyEnforceIf

operations\_research::sat::Constraint

operations\_research::sat::Constraint

OnlyEnforceIf

`Constraint operations_research::sat::Constraint::OnlyEnforceIf (absl::Span< const BoolVar > literals)`

The constraint will be enforced iff all literals listed here are true.
If this is empty, then the constraint will always be enforced. An
enforced constraint must be satisfied, and an un-enforced one will
simply be ignored.

This is also called half-reification. To have an equivalence between a
literal and a constraint (full reification), one must add both a
constraint (controlled by a literal l) and its negation (controlled by
the negation of l).

Important: as of September 2018, only a few constraint support
enforcement:

-   bool\_or, bool\_and, linear: fully supported.

-   interval: only support a single enforcement literal.

-   other: no support (but can be added on a per-demand basis).

### OnlyEnforceIf()`[2/2]`

OnlyEnforceIf

operations\_research::sat::Constraint

operations\_research::sat::Constraint

OnlyEnforceIf

`Constraint operations_research::sat::Constraint::OnlyEnforceIf (BoolVar literal)`

See [OnlyEnforceIf(absl::Span\<const BoolVar\>
literals)](#_classoperations__research_1_1sat_1_1Constraint_1a9052e9e1dd8248433909b5542f314add).

### Proto()

Proto

operations\_research::sat::Constraint

operations\_research::sat::Constraint

Proto

`const ConstraintProto& operations_research::sat::Constraint::Proto ( ) const[inline]`

Useful for testing.

### WithName()

WithName

operations\_research::sat::Constraint

operations\_research::sat::Constraint

WithName

`Constraint operations_research::sat::Constraint::WithName (const std::string & name)`

Sets the name of the constraint.

Friends And Related Function Documentation
------------------------------------------

### CpModelBuilder

CpModelBuilder

operations\_research::sat::Constraint

operations\_research::sat::Constraint

CpModelBuilder

`friend class CpModelBuilder[friend]`

Member Data Documentation
-------------------------

### proto\_

proto\_

operations\_research::sat::Constraint

operations\_research::sat::Constraint

proto\_

`ConstraintProto* operations_research::sat::Constraint::proto_[protected]`

The documentation for this class was generated from the following file:

ortools/sat/

cp\_model.h

operations\_research::sat::CpModelBuilder Class Reference {#_classoperations__research_1_1sat_1_1CpModelBuilder}
=========================================================

operations\_research::sat::CpModelBuilder

`#include <cp_model.h>`

-   [IntVar](#_classoperations__research_1_1sat_1_1IntVar)
    [NewIntVar](#_classoperations__research_1_1sat_1_1CpModelBuilder_1a0887a2fe4518bde7bbde18f592b6243f)
    (const [Domain](#_classoperations__research_1_1Domain) &domain)

    Creates an integer variable with the given domain.

-   [BoolVar](#_classoperations__research_1_1sat_1_1BoolVar)
    [NewBoolVar](#_classoperations__research_1_1sat_1_1CpModelBuilder_1a8fc4a0c717f985687d63a586dba04641)
    ()

    Creates a Boolean variable.

-   [IntVar](#_classoperations__research_1_1sat_1_1IntVar)
    [NewConstant](#_classoperations__research_1_1sat_1_1CpModelBuilder_1adac551c8b80fc7bdd7b30779fd20a4ea)
    (int64 value)

    Creates a constant variable.

-   [BoolVar](#_classoperations__research_1_1sat_1_1BoolVar)
    [TrueVar](#_classoperations__research_1_1sat_1_1CpModelBuilder_1a6dc655a67c5213fcefb82a213dac5e2c)
    ()

    Creates an always true Boolean variable.

-   [BoolVar](#_classoperations__research_1_1sat_1_1BoolVar)
    [FalseVar](#_classoperations__research_1_1sat_1_1CpModelBuilder_1a9a53531099bebddbf54dd15418817326)
    ()

    Creates an always false Boolean variable.

-   [IntervalVar](#_classoperations__research_1_1sat_1_1IntervalVar)
    [NewIntervalVar](#_classoperations__research_1_1sat_1_1CpModelBuilder_1a4e1c85e161ee8e50f2f2162cd7294d03)
    ([IntVar](#_classoperations__research_1_1sat_1_1IntVar) start,
    [IntVar](#_classoperations__research_1_1sat_1_1IntVar) size,
    [IntVar](#_classoperations__research_1_1sat_1_1IntVar) end)

    Creates an interval variable.

-   [IntervalVar](#_classoperations__research_1_1sat_1_1IntervalVar)
    [NewOptionalIntervalVar](#_classoperations__research_1_1sat_1_1CpModelBuilder_1a99c82eca478306942b3aed3372b38384)
    ([IntVar](#_classoperations__research_1_1sat_1_1IntVar) start,
    [IntVar](#_classoperations__research_1_1sat_1_1IntVar) size,
    [IntVar](#_classoperations__research_1_1sat_1_1IntVar) end,
    [BoolVar](#_classoperations__research_1_1sat_1_1BoolVar) presence)

    Creates an optional interval variable.

-   [Constraint](#_classoperations__research_1_1sat_1_1Constraint)
    [AddBoolOr](#_classoperations__research_1_1sat_1_1CpModelBuilder_1ae8bd984917b305dc49abae6c19b69ea3)
    (absl::Span\< const
    [BoolVar](#_classoperations__research_1_1sat_1_1BoolVar) \>
    literals)

    Adds the constraint that at least one of the literals must be true.

-   [Constraint](#_classoperations__research_1_1sat_1_1Constraint)
    [AddBoolAnd](#_classoperations__research_1_1sat_1_1CpModelBuilder_1a3088d984ab4874140f7c367dc457ac0f)
    (absl::Span\< const
    [BoolVar](#_classoperations__research_1_1sat_1_1BoolVar) \>
    literals)

    Adds the constraint that all literals must be true.

-   [Constraint](#_classoperations__research_1_1sat_1_1Constraint)
    [AddBoolXor](#_classoperations__research_1_1sat_1_1CpModelBuilder_1a18d2ca2be01dd3e67893f4e1dbe4af43)
    (absl::Span\< const
    [BoolVar](#_classoperations__research_1_1sat_1_1BoolVar) \>
    literals)

    Adds the constraint that a odd number of literal is true.

-   [Constraint](#_classoperations__research_1_1sat_1_1Constraint)
    [AddImplication](#_classoperations__research_1_1sat_1_1CpModelBuilder_1a43ca3f9c073ea5078c1abd3bb0c563d4)
    ([BoolVar](#_classoperations__research_1_1sat_1_1BoolVar) a,
    [BoolVar](#_classoperations__research_1_1sat_1_1BoolVar) b)

    Adds a =\> b.

-   [Constraint](#_classoperations__research_1_1sat_1_1Constraint)
    [AddEquality](#_classoperations__research_1_1sat_1_1CpModelBuilder_1ad941d4f0156fc746c4ed12790bce7af7)
    (const
    [LinearExpr](#_classoperations__research_1_1sat_1_1LinearExpr)
    &left, const
    [LinearExpr](#_classoperations__research_1_1sat_1_1LinearExpr)
    &right)

    Adds left == right.

-   [Constraint](#_classoperations__research_1_1sat_1_1Constraint)
    [AddGreaterOrEqual](#_classoperations__research_1_1sat_1_1CpModelBuilder_1a7a718730caef4f258e1cbbb2e3e3b452)
    (const
    [LinearExpr](#_classoperations__research_1_1sat_1_1LinearExpr)
    &left, const
    [LinearExpr](#_classoperations__research_1_1sat_1_1LinearExpr)
    &right)

    Adds left \>= right.

-   [Constraint](#_classoperations__research_1_1sat_1_1Constraint)
    [AddGreaterThan](#_classoperations__research_1_1sat_1_1CpModelBuilder_1acf4c5429ec08207e147b65bd1330ba92)
    (const
    [LinearExpr](#_classoperations__research_1_1sat_1_1LinearExpr)
    &left, const
    [LinearExpr](#_classoperations__research_1_1sat_1_1LinearExpr)
    &right)

    Adds left \> right.

-   [Constraint](#_classoperations__research_1_1sat_1_1Constraint)
    [AddLessOrEqual](#_classoperations__research_1_1sat_1_1CpModelBuilder_1a4f1c8c11f9f840728e5c037249192b8f)
    (const
    [LinearExpr](#_classoperations__research_1_1sat_1_1LinearExpr)
    &left, const
    [LinearExpr](#_classoperations__research_1_1sat_1_1LinearExpr)
    &right)

    Adds left \<= right.

-   [Constraint](#_classoperations__research_1_1sat_1_1Constraint)
    [AddLessThan](#_classoperations__research_1_1sat_1_1CpModelBuilder_1a7cf9ff9df25ff433286b4f5bda41f990)
    (const
    [LinearExpr](#_classoperations__research_1_1sat_1_1LinearExpr)
    &left, const
    [LinearExpr](#_classoperations__research_1_1sat_1_1LinearExpr)
    &right)

    Adds left \< right.

-   [Constraint](#_classoperations__research_1_1sat_1_1Constraint)
    [AddLinearConstraint](#_classoperations__research_1_1sat_1_1CpModelBuilder_1a562a899753d60f28ae87ecb93e96b797)
    (const
    [LinearExpr](#_classoperations__research_1_1sat_1_1LinearExpr)
    &expr, const [Domain](#_classoperations__research_1_1Domain)
    &domain)

    Adds expr in domain.

-   [Constraint](#_classoperations__research_1_1sat_1_1Constraint)
    [AddNotEqual](#_classoperations__research_1_1sat_1_1CpModelBuilder_1aa64c33dd1487bf4f0d575edf33ef2dc9)
    (const
    [LinearExpr](#_classoperations__research_1_1sat_1_1LinearExpr)
    &left, const
    [LinearExpr](#_classoperations__research_1_1sat_1_1LinearExpr)
    &right)

    Adds left != right.

-   [Constraint](#_classoperations__research_1_1sat_1_1Constraint)
    [AddAllDifferent](#_classoperations__research_1_1sat_1_1CpModelBuilder_1a605cc0b904f4d9b2de5fffbf6fa40c68)
    (absl::Span\< const
    [IntVar](#_classoperations__research_1_1sat_1_1IntVar) \> vars)

    this constraint forces all variables to have different values.

-   [Constraint](#_classoperations__research_1_1sat_1_1Constraint)
    [AddVariableElement](#_classoperations__research_1_1sat_1_1CpModelBuilder_1a001974a3f1f5e9d791ae10cd435f07cf)
    ([IntVar](#_classoperations__research_1_1sat_1_1IntVar) index,
    absl::Span\< const
    [IntVar](#_classoperations__research_1_1sat_1_1IntVar) \> variables,
    [IntVar](#_classoperations__research_1_1sat_1_1IntVar) target)

    Adds the element constraint: variables\[index\] == target.

-   [Constraint](#_classoperations__research_1_1sat_1_1Constraint)
    [AddElement](#_classoperations__research_1_1sat_1_1CpModelBuilder_1ada1b4fad9b4f017f9009ce3761123a8b)
    ([IntVar](#_classoperations__research_1_1sat_1_1IntVar) index,
    absl::Span\< const int64 \> values,
    [IntVar](#_classoperations__research_1_1sat_1_1IntVar) target)

    Adds the element constraint: values\[index\] == target.

-   [CircuitConstraint](#_classoperations__research_1_1sat_1_1CircuitConstraint)
    [AddCircuitConstraint](#_classoperations__research_1_1sat_1_1CpModelBuilder_1ad5ec615a9107ebcb8a7516bb3ccfbcd2)
    ()

-   [TableConstraint](#_classoperations__research_1_1sat_1_1TableConstraint)
    [AddAllowedAssignments](#_classoperations__research_1_1sat_1_1CpModelBuilder_1a7d05d91ffdd70f16ad170e25fd47e200)
    (absl::Span\< const
    [IntVar](#_classoperations__research_1_1sat_1_1IntVar) \> vars)

-   [TableConstraint](#_classoperations__research_1_1sat_1_1TableConstraint)
    [AddForbiddenAssignments](#_classoperations__research_1_1sat_1_1CpModelBuilder_1a05b1310e7cfde91fbdc10798a84a2345)
    (absl::Span\< const
    [IntVar](#_classoperations__research_1_1sat_1_1IntVar) \> vars)

-   [Constraint](#_classoperations__research_1_1sat_1_1Constraint)
    [AddInverseConstraint](#_classoperations__research_1_1sat_1_1CpModelBuilder_1a0c391768bc423a43875a7867ee247a4b)
    (absl::Span\< const
    [IntVar](#_classoperations__research_1_1sat_1_1IntVar) \> variables,
    absl::Span\< const
    [IntVar](#_classoperations__research_1_1sat_1_1IntVar) \>
    inverse\_variables)

-   [ReservoirConstraint](#_classoperations__research_1_1sat_1_1ReservoirConstraint)
    [AddReservoirConstraint](#_classoperations__research_1_1sat_1_1CpModelBuilder_1a5d2c35d16d6b9cb25254ca6d3b963ac8)
    (int64 min\_level, int64 max\_level)

-   [AutomatonConstraint](#_classoperations__research_1_1sat_1_1AutomatonConstraint)
    [AddAutomaton](#_classoperations__research_1_1sat_1_1CpModelBuilder_1a5738c98c07c2e0ec747877eb3813a134)
    (absl::Span\< const
    [IntVar](#_classoperations__research_1_1sat_1_1IntVar) \>
    transition\_variables, int starting\_state, absl::Span\< const
    int \> final\_states)

-   [Constraint](#_classoperations__research_1_1sat_1_1Constraint)
    [AddMinEquality](#_classoperations__research_1_1sat_1_1CpModelBuilder_1a967f11af5e1cfb143514e09925628be5)
    ([IntVar](#_classoperations__research_1_1sat_1_1IntVar) target,
    absl::Span\< const
    [IntVar](#_classoperations__research_1_1sat_1_1IntVar) \> vars)

    Adds target == min(vars).

-   [Constraint](#_classoperations__research_1_1sat_1_1Constraint)
    [AddMaxEquality](#_classoperations__research_1_1sat_1_1CpModelBuilder_1a902eb5d208511f7da9cdd9cde9a79c45)
    ([IntVar](#_classoperations__research_1_1sat_1_1IntVar) target,
    absl::Span\< const
    [IntVar](#_classoperations__research_1_1sat_1_1IntVar) \> vars)

    Adds target == max(vars).

-   [Constraint](#_classoperations__research_1_1sat_1_1Constraint)
    [AddDivisionEquality](#_classoperations__research_1_1sat_1_1CpModelBuilder_1adffb8e57735762a6f321279f2e60ae65)
    ([IntVar](#_classoperations__research_1_1sat_1_1IntVar) target,
    [IntVar](#_classoperations__research_1_1sat_1_1IntVar) numerator,
    [IntVar](#_classoperations__research_1_1sat_1_1IntVar) denominator)

    Adds target = num / denom (integer division rounded towards 0).

-   [Constraint](#_classoperations__research_1_1sat_1_1Constraint)
    [AddAbsEquality](#_classoperations__research_1_1sat_1_1CpModelBuilder_1aa1eae45130c127fe6cac9805736216ef)
    ([IntVar](#_classoperations__research_1_1sat_1_1IntVar) target,
    [IntVar](#_classoperations__research_1_1sat_1_1IntVar) var)

    Adds target == abs(var).

-   [Constraint](#_classoperations__research_1_1sat_1_1Constraint)
    [AddModuloEquality](#_classoperations__research_1_1sat_1_1CpModelBuilder_1abd73201c6fbc455ca4783ff99ca2eed1)
    ([IntVar](#_classoperations__research_1_1sat_1_1IntVar) target,
    [IntVar](#_classoperations__research_1_1sat_1_1IntVar) var,
    [IntVar](#_classoperations__research_1_1sat_1_1IntVar) mod)

    Adds target = var % mod.

-   [Constraint](#_classoperations__research_1_1sat_1_1Constraint)
    [AddProductEquality](#_classoperations__research_1_1sat_1_1CpModelBuilder_1a991b6a2a16def3962ccc5727004638db)
    ([IntVar](#_classoperations__research_1_1sat_1_1IntVar) target,
    absl::Span\< const
    [IntVar](#_classoperations__research_1_1sat_1_1IntVar) \> vars)

    Adds target == prod(vars).

-   [Constraint](#_classoperations__research_1_1sat_1_1Constraint)
    [AddNoOverlap](#_classoperations__research_1_1sat_1_1CpModelBuilder_1a89c4590eaf404f0ef3b80d4ce584fbda)
    (absl::Span\< const
    [IntervalVar](#_classoperations__research_1_1sat_1_1IntervalVar) \>
    vars)

-   [NoOverlap2DConstraint](#_classoperations__research_1_1sat_1_1NoOverlap2DConstraint)
    [AddNoOverlap2D](#_classoperations__research_1_1sat_1_1CpModelBuilder_1a10d61bc6bc9584cadfc0b87537ada9eb)
    ()

    The no\_overlap\_2d constraint prevents a set of boxes from
    overlapping.

-   [CumulativeConstraint](#_classoperations__research_1_1sat_1_1CumulativeConstraint)
    [AddCumulative](#_classoperations__research_1_1sat_1_1CpModelBuilder_1a6620906cabb980393d9433df9a7f7b70)
    ([IntVar](#_classoperations__research_1_1sat_1_1IntVar) capacity)

-   void
    [Minimize](#_classoperations__research_1_1sat_1_1CpModelBuilder_1a0faf578c69fe9ae80ee0ea9f671dc5e7)
    (const
    [LinearExpr](#_classoperations__research_1_1sat_1_1LinearExpr)
    &expr)

    Adds a linear minimization objective.

-   void
    [Maximize](#_classoperations__research_1_1sat_1_1CpModelBuilder_1a3559ac1f9f840b2d5637f1d26cd18f0b)
    (const
    [LinearExpr](#_classoperations__research_1_1sat_1_1LinearExpr)
    &expr)

    Adds a linear maximization objective.

-   void
    [ScaleObjectiveBy](#_classoperations__research_1_1sat_1_1CpModelBuilder_1ac93a7c7467278afb9eac2bb4a8dec6d3)
    (double scaling)

-   void
    [AddDecisionStrategy](#_classoperations__research_1_1sat_1_1CpModelBuilder_1a4d0cfb231f4bed2420d0aff928f3a980)
    (absl::Span\< const
    [IntVar](#_classoperations__research_1_1sat_1_1IntVar) \> variables,
    DecisionStrategyProto::VariableSelectionStrategy var\_strategy,
    DecisionStrategyProto::DomainReductionStrategy domain\_strategy)

    Adds a decision strategy on a list of integer variables.

-   void
    [AddDecisionStrategy](#_classoperations__research_1_1sat_1_1CpModelBuilder_1a81d8ef14e29732b81f56778090bfa547)
    (absl::Span\< const
    [BoolVar](#_classoperations__research_1_1sat_1_1BoolVar) \>
    variables, DecisionStrategyProto::VariableSelectionStrategy
    var\_strategy, DecisionStrategyProto::DomainReductionStrategy
    domain\_strategy)

    Adds a decision strategy on a list of boolean variables.

-   const CpModelProto &
    [Build](#_classoperations__research_1_1sat_1_1CpModelBuilder_1a8e1b64644f124be491431bbae9d5d843)
    () const

-   const CpModelProto &
    [Proto](#_classoperations__research_1_1sat_1_1CpModelBuilder_1a791f54d4afefc05d6462fa9a9f1f304d)
    () const

-   CpModelProto \*
    [MutableProto](#_classoperations__research_1_1sat_1_1CpModelBuilder_1a4b3320604b4344b5bea17c5fae1ed7ce)
    ()

<!-- -->

-   class
    [CumulativeConstraint](#_classoperations__research_1_1sat_1_1CpModelBuilder_1a9d31ad87d4edee55fc3cb5e239077720)

-   class
    [ReservoirConstraint](#_classoperations__research_1_1sat_1_1CpModelBuilder_1ae0ff478f6506cb705bbc1737598276f4)

Detailed Description
--------------------

Wrapper class around the cp\_model proto.

This class provides two types of methods:

-   NewXXX to create integer, boolean, or interval variables.

-   AddXXX to create new constraints and add them to the model.

Member Function Documentation
-----------------------------

### AddAbsEquality()

AddAbsEquality

operations\_research::sat::CpModelBuilder

operations\_research::sat::CpModelBuilder

AddAbsEquality

`Constraint operations_research::sat::CpModelBuilder::AddAbsEquality (IntVar target, IntVar var)`

Adds target == abs(var).

### AddAllDifferent()

AddAllDifferent

operations\_research::sat::CpModelBuilder

operations\_research::sat::CpModelBuilder

AddAllDifferent

`Constraint operations_research::sat::CpModelBuilder::AddAllDifferent (absl::Span< const IntVar > vars)`

this constraint forces all variables to have different values.

### AddAllowedAssignments()

AddAllowedAssignments

operations\_research::sat::CpModelBuilder

operations\_research::sat::CpModelBuilder

AddAllowedAssignments

`TableConstraint operations_research::sat::CpModelBuilder::AddAllowedAssignments (absl::Span< const IntVar > vars)`

Adds an allowed assignments constraint.

An AllowedAssignments constraint is a constraint on an array of
variables that forces, when all variables are fixed to a single value,
that the corresponding list of values is equal to one of the tuple added
to the constraint.

It returns a table constraint that allows adding tuples incrementally
after construction,

### AddAutomaton()

AddAutomaton

operations\_research::sat::CpModelBuilder

operations\_research::sat::CpModelBuilder

AddAutomaton

`AutomatonConstraint operations_research::sat::CpModelBuilder::AddAutomaton (absl::Span< const IntVar > transition_variables, int starting_state, absl::Span< const int > final_states)`

An automaton constraint takes a list of variables (of size n), an
initial state, a set of final states, and a set of transitions. A
transition is a triplet (\'tail\', \'head\', \'label\'), where \'tail\'
and \'head\' are states, and \'label\' is the label of an arc from
\'head\' to \'tail\', corresponding to the value of one variable in the
list of variables.

This automaton will be unrolled into a flow with n + 1 phases. Each
phase contains the possible states of the automaton. The first state
contains the initial state. The last phase contains the final states.

Between two consecutive phases i and i + 1, the automaton creates a set
of arcs. For each transition (tail, head, label), it will add an arc
from the state \'tail\' of phase i and the state \'head\' of phase i +
1. This arc labeled by the value \'label\' of the variables
\'variables\[i\]\'. That is, this arc can only be selected if
\'variables\[i\]\' is assigned the value \'label\'. A feasible solution
of this constraint is an assignment of variables such that, starting
from the initial state in phase 0, there is a path labeled by the values
of the variables that ends in one of the final states in the final
phase.

It returns an
[AutomatonConstraint](#_classoperations__research_1_1sat_1_1AutomatonConstraint)
that allows adding transition incrementally after construction.

### AddBoolAnd()

AddBoolAnd

operations\_research::sat::CpModelBuilder

operations\_research::sat::CpModelBuilder

AddBoolAnd

`Constraint operations_research::sat::CpModelBuilder::AddBoolAnd (absl::Span< const BoolVar > literals)`

Adds the constraint that all literals must be true.

### AddBoolOr()

AddBoolOr

operations\_research::sat::CpModelBuilder

operations\_research::sat::CpModelBuilder

AddBoolOr

`Constraint operations_research::sat::CpModelBuilder::AddBoolOr (absl::Span< const BoolVar > literals)`

Adds the constraint that at least one of the literals must be true.

### AddBoolXor()

AddBoolXor

operations\_research::sat::CpModelBuilder

operations\_research::sat::CpModelBuilder

AddBoolXor

`Constraint operations_research::sat::CpModelBuilder::AddBoolXor (absl::Span< const BoolVar > literals)`

Adds the constraint that a odd number of literal is true.

### AddCircuitConstraint()

AddCircuitConstraint

operations\_research::sat::CpModelBuilder

operations\_research::sat::CpModelBuilder

AddCircuitConstraint

`CircuitConstraint operations_research::sat::CpModelBuilder::AddCircuitConstraint ( )`

Adds a circuit constraint.

The circuit constraint is defined on a graph where the arc presence are
controlled by literals. That is the arc is part of the circuit of its
corresponding literal is assigned to true.

For now, we ignore node indices with no incident arc. All the other
nodes must have exactly one incoming and one outgoing selected arc (i.e.
literal at true). All the selected arcs that are not self-loops must
form a single circuit.

It returns a circuit constraint that allows adding arcs incrementally
after construction.

### AddCumulative()

AddCumulative

operations\_research::sat::CpModelBuilder

operations\_research::sat::CpModelBuilder

AddCumulative

`CumulativeConstraint operations_research::sat::CpModelBuilder::AddCumulative (IntVar capacity)`

The cumulative constraint ensures that for any integer point, the sum of
the demands of the intervals containing that point does not exceed the
capacity.

### AddDecisionStrategy()`[1/2]`

AddDecisionStrategy

operations\_research::sat::CpModelBuilder

operations\_research::sat::CpModelBuilder

AddDecisionStrategy

`void operations_research::sat::CpModelBuilder::AddDecisionStrategy (absl::Span< const IntVar > variables, DecisionStrategyProto::VariableSelectionStrategy var_strategy, DecisionStrategyProto::DomainReductionStrategy domain_strategy)`

Adds a decision strategy on a list of integer variables.

### AddDecisionStrategy()`[2/2]`

AddDecisionStrategy

operations\_research::sat::CpModelBuilder

operations\_research::sat::CpModelBuilder

AddDecisionStrategy

`void operations_research::sat::CpModelBuilder::AddDecisionStrategy (absl::Span< const BoolVar > variables, DecisionStrategyProto::VariableSelectionStrategy var_strategy, DecisionStrategyProto::DomainReductionStrategy domain_strategy)`

Adds a decision strategy on a list of boolean variables.

### AddDivisionEquality()

AddDivisionEquality

operations\_research::sat::CpModelBuilder

operations\_research::sat::CpModelBuilder

AddDivisionEquality

`Constraint operations_research::sat::CpModelBuilder::AddDivisionEquality (IntVar target, IntVar numerator, IntVar denominator)`

Adds target = num / denom (integer division rounded towards 0).

### AddElement()

AddElement

operations\_research::sat::CpModelBuilder

operations\_research::sat::CpModelBuilder

AddElement

`Constraint operations_research::sat::CpModelBuilder::AddElement (IntVar index, absl::Span< const int64 > values, IntVar target)`

Adds the element constraint: values\[index\] == target.

### AddEquality()

AddEquality

operations\_research::sat::CpModelBuilder

operations\_research::sat::CpModelBuilder

AddEquality

`Constraint operations_research::sat::CpModelBuilder::AddEquality (const LinearExpr & left, const LinearExpr & right)`

Adds left == right.

### AddForbiddenAssignments()

AddForbiddenAssignments

operations\_research::sat::CpModelBuilder

operations\_research::sat::CpModelBuilder

AddForbiddenAssignments

`TableConstraint operations_research::sat::CpModelBuilder::AddForbiddenAssignments (absl::Span< const IntVar > vars)`

Adds an forbidden assignments constraint.

A ForbiddenAssignments constraint is a constraint on an array of
variables where the list of impossible combinations is provided in the
tuples added to the constraint.

It returns a table constraint that allows adding tuples incrementally
after construction,

### AddGreaterOrEqual()

AddGreaterOrEqual

operations\_research::sat::CpModelBuilder

operations\_research::sat::CpModelBuilder

AddGreaterOrEqual

`Constraint operations_research::sat::CpModelBuilder::AddGreaterOrEqual (const LinearExpr & left, const LinearExpr & right)`

Adds left \>= right.

### AddGreaterThan()

AddGreaterThan

operations\_research::sat::CpModelBuilder

operations\_research::sat::CpModelBuilder

AddGreaterThan

`Constraint operations_research::sat::CpModelBuilder::AddGreaterThan (const LinearExpr & left, const LinearExpr & right)`

Adds left \> right.

### AddImplication()

AddImplication

operations\_research::sat::CpModelBuilder

operations\_research::sat::CpModelBuilder

AddImplication

`Constraint operations_research::sat::CpModelBuilder::AddImplication (BoolVar a, BoolVar b)[inline]`

Adds a =\> b.

### AddInverseConstraint()

AddInverseConstraint

operations\_research::sat::CpModelBuilder

operations\_research::sat::CpModelBuilder

AddInverseConstraint

`Constraint operations_research::sat::CpModelBuilder::AddInverseConstraint (absl::Span< const IntVar > variables, absl::Span< const IntVar > inverse_variables)`

An inverse constraint enforces that if \'variables\[i\]\' is assigned a
value \'j\', then inverse\_variables\[j\] is assigned a value \'i\'. And
vice versa.

### AddLessOrEqual()

AddLessOrEqual

operations\_research::sat::CpModelBuilder

operations\_research::sat::CpModelBuilder

AddLessOrEqual

`Constraint operations_research::sat::CpModelBuilder::AddLessOrEqual (const LinearExpr & left, const LinearExpr & right)`

Adds left \<= right.

### AddLessThan()

AddLessThan

operations\_research::sat::CpModelBuilder

operations\_research::sat::CpModelBuilder

AddLessThan

`Constraint operations_research::sat::CpModelBuilder::AddLessThan (const LinearExpr & left, const LinearExpr & right)`

Adds left \< right.

### AddLinearConstraint()

AddLinearConstraint

operations\_research::sat::CpModelBuilder

operations\_research::sat::CpModelBuilder

AddLinearConstraint

`Constraint operations_research::sat::CpModelBuilder::AddLinearConstraint (const LinearExpr & expr, const Domain & domain)`

Adds expr in domain.

### AddMaxEquality()

AddMaxEquality

operations\_research::sat::CpModelBuilder

operations\_research::sat::CpModelBuilder

AddMaxEquality

`Constraint operations_research::sat::CpModelBuilder::AddMaxEquality (IntVar target, absl::Span< const IntVar > vars)`

Adds target == max(vars).

### AddMinEquality()

AddMinEquality

operations\_research::sat::CpModelBuilder

operations\_research::sat::CpModelBuilder

AddMinEquality

`Constraint operations_research::sat::CpModelBuilder::AddMinEquality (IntVar target, absl::Span< const IntVar > vars)`

Adds target == min(vars).

### AddModuloEquality()

AddModuloEquality

operations\_research::sat::CpModelBuilder

operations\_research::sat::CpModelBuilder

AddModuloEquality

`Constraint operations_research::sat::CpModelBuilder::AddModuloEquality (IntVar target, IntVar var, IntVar mod)`

Adds target = var % mod.

### AddNoOverlap()

AddNoOverlap

operations\_research::sat::CpModelBuilder

operations\_research::sat::CpModelBuilder

AddNoOverlap

`Constraint operations_research::sat::CpModelBuilder::AddNoOverlap (absl::Span< const IntervalVar > vars)`

Adds a constraint than ensures that all present intervals do not overlap
in time.

### AddNoOverlap2D()

AddNoOverlap2D

operations\_research::sat::CpModelBuilder

operations\_research::sat::CpModelBuilder

AddNoOverlap2D

`NoOverlap2DConstraint operations_research::sat::CpModelBuilder::AddNoOverlap2D ( )`

The no\_overlap\_2d constraint prevents a set of boxes from overlapping.

### AddNotEqual()

AddNotEqual

operations\_research::sat::CpModelBuilder

operations\_research::sat::CpModelBuilder

AddNotEqual

`Constraint operations_research::sat::CpModelBuilder::AddNotEqual (const LinearExpr & left, const LinearExpr & right)`

Adds left != right.

### AddProductEquality()

AddProductEquality

operations\_research::sat::CpModelBuilder

operations\_research::sat::CpModelBuilder

AddProductEquality

`Constraint operations_research::sat::CpModelBuilder::AddProductEquality (IntVar target, absl::Span< const IntVar > vars)`

Adds target == prod(vars).

### AddReservoirConstraint()

AddReservoirConstraint

operations\_research::sat::CpModelBuilder

operations\_research::sat::CpModelBuilder

AddReservoirConstraint

`ReservoirConstraint operations_research::sat::CpModelBuilder::AddReservoirConstraint (int64 min_level, int64 max_level)`

Adds a reservoir constraint with optional refill/emptying events.

Maintain a reservoir level within bounds. The water level starts at 0,
and at any time \>= 0, it must be within min\_level, and max\_level.
Furthermore, this constraints expect all times variables to be \>= 0.
Given an event (time, demand, active), if active is true, and if time is
assigned a value t, then the level of the reservoir changes by demand
(which is constant) at time t.

> **Note**
>
> level\_min can be \> 0, or level\_max can be \< 0. It just forces some
> demands to be executed at time 0 to make sure that we are within those
> bounds with the executed demands. Therefore, at any time t \>= 0:
> sum(demands\[i\] \* actives\[i\] if times\[i\] \<= t) in \[min\_level,
> max\_level\]

It returns a
[ReservoirConstraint](#_classoperations__research_1_1sat_1_1ReservoirConstraint)
that allows adding optional and non optional events incrementally after
construction.

### AddVariableElement()

AddVariableElement

operations\_research::sat::CpModelBuilder

operations\_research::sat::CpModelBuilder

AddVariableElement

`Constraint operations_research::sat::CpModelBuilder::AddVariableElement (IntVar index, absl::Span< const IntVar > variables, IntVar target)`

Adds the element constraint: variables\[index\] == target.

### Build()

Build

operations\_research::sat::CpModelBuilder

operations\_research::sat::CpModelBuilder

Build

`const CpModelProto& operations_research::sat::CpModelBuilder::Build ( ) const[inline]`

[Todo](#_todo_1_todo000002)

(user) : add MapDomain?

### FalseVar()

FalseVar

operations\_research::sat::CpModelBuilder

operations\_research::sat::CpModelBuilder

FalseVar

`BoolVar operations_research::sat::CpModelBuilder::FalseVar ( )`

Creates an always false Boolean variable.

### Maximize()

Maximize

operations\_research::sat::CpModelBuilder

operations\_research::sat::CpModelBuilder

Maximize

`void operations_research::sat::CpModelBuilder::Maximize (const LinearExpr & expr)`

Adds a linear maximization objective.

### Minimize()

Minimize

operations\_research::sat::CpModelBuilder

operations\_research::sat::CpModelBuilder

Minimize

`void operations_research::sat::CpModelBuilder::Minimize (const LinearExpr & expr)`

Adds a linear minimization objective.

### MutableProto()

MutableProto

operations\_research::sat::CpModelBuilder

operations\_research::sat::CpModelBuilder

MutableProto

`CpModelProto* operations_research::sat::CpModelBuilder::MutableProto ( )[inline]`

### NewBoolVar()

NewBoolVar

operations\_research::sat::CpModelBuilder

operations\_research::sat::CpModelBuilder

NewBoolVar

`BoolVar operations_research::sat::CpModelBuilder::NewBoolVar ( )`

Creates a Boolean variable.

### NewConstant()

NewConstant

operations\_research::sat::CpModelBuilder

operations\_research::sat::CpModelBuilder

NewConstant

`IntVar operations_research::sat::CpModelBuilder::NewConstant (int64 value)`

Creates a constant variable.

### NewIntervalVar()

NewIntervalVar

operations\_research::sat::CpModelBuilder

operations\_research::sat::CpModelBuilder

NewIntervalVar

`IntervalVar operations_research::sat::CpModelBuilder::NewIntervalVar (IntVar start, IntVar size, IntVar end)`

Creates an interval variable.

### NewIntVar()

NewIntVar

operations\_research::sat::CpModelBuilder

operations\_research::sat::CpModelBuilder

NewIntVar

`IntVar operations_research::sat::CpModelBuilder::NewIntVar (const Domain & domain)`

Creates an integer variable with the given domain.

### NewOptionalIntervalVar()

NewOptionalIntervalVar

operations\_research::sat::CpModelBuilder

operations\_research::sat::CpModelBuilder

NewOptionalIntervalVar

`IntervalVar operations_research::sat::CpModelBuilder::NewOptionalIntervalVar (IntVar start, IntVar size, IntVar end, BoolVar presence)`

Creates an optional interval variable.

### Proto()

Proto

operations\_research::sat::CpModelBuilder

operations\_research::sat::CpModelBuilder

Proto

`const CpModelProto& operations_research::sat::CpModelBuilder::Proto ( ) const[inline]`

### ScaleObjectiveBy()

ScaleObjectiveBy

operations\_research::sat::CpModelBuilder

operations\_research::sat::CpModelBuilder

ScaleObjectiveBy

`void operations_research::sat::CpModelBuilder::ScaleObjectiveBy (double scaling)`

Sets scaling of the objective. (must be called after
[Minimize()](#_classoperations__research_1_1sat_1_1CpModelBuilder_1a0faf578c69fe9ae80ee0ea9f671dc5e7)
of
[Maximize()](#_classoperations__research_1_1sat_1_1CpModelBuilder_1a3559ac1f9f840b2d5637f1d26cd18f0b)).
\'scaling\' must be \> 0.0.

### TrueVar()

TrueVar

operations\_research::sat::CpModelBuilder

operations\_research::sat::CpModelBuilder

TrueVar

`BoolVar operations_research::sat::CpModelBuilder::TrueVar ( )`

Creates an always true Boolean variable.

Friends And Related Function Documentation
------------------------------------------

### CumulativeConstraint

CumulativeConstraint

operations\_research::sat::CpModelBuilder

operations\_research::sat::CpModelBuilder

CumulativeConstraint

`friend class CumulativeConstraint[friend]`

### ReservoirConstraint

ReservoirConstraint

operations\_research::sat::CpModelBuilder

operations\_research::sat::CpModelBuilder

ReservoirConstraint

`friend class ReservoirConstraint[friend]`

The documentation for this class was generated from the following file:

ortools/sat/

cp\_model.h

operations\_research::sat::CumulativeConstraint Class Reference {#_classoperations__research_1_1sat_1_1CumulativeConstraint}
===============================================================

operations\_research::sat::CumulativeConstraint

`#include <cp_model.h>`

Inheritance diagram for operations\_research::sat::CumulativeConstraint:

Collaboration diagram for
operations\_research::sat::CumulativeConstraint:

-   void
    [AddDemand](#_classoperations__research_1_1sat_1_1CumulativeConstraint_1aded0689c7c92b1a7739758150131b531)
    ([IntervalVar](#_classoperations__research_1_1sat_1_1IntervalVar)
    interval, [IntVar](#_classoperations__research_1_1sat_1_1IntVar)
    demand)

<!-- -->

-   class
    [CpModelBuilder](#_classoperations__research_1_1sat_1_1CumulativeConstraint_1ae04c85577cf33a05fb50bb361877fb42)

Detailed Description
--------------------

Specialized cumulative constraint.

This constraint allows adding fixed or variables demands to the
cumulative constraint incrementally.

Member Function Documentation
-----------------------------

### AddDemand()

AddDemand

operations\_research::sat::CumulativeConstraint

operations\_research::sat::CumulativeConstraint

AddDemand

`void operations_research::sat::CumulativeConstraint::AddDemand (IntervalVar interval, IntVar demand)`

Friends And Related Function Documentation
------------------------------------------

### CpModelBuilder

CpModelBuilder

operations\_research::sat::CumulativeConstraint

operations\_research::sat::CumulativeConstraint

CpModelBuilder

`friend class CpModelBuilder[friend]`

The documentation for this class was generated from the following file:

ortools/sat/

cp\_model.h

operations\_research::sat::IntVar Class Reference {#_classoperations__research_1_1sat_1_1IntVar}
=================================================

operations\_research::sat::IntVar

`#include <cp_model.h>`

-   [IntVar](#_classoperations__research_1_1sat_1_1IntVar_1a47cdd55b99ca5d29b194f54b14889819)
    ()

-   [IntVar](#_classoperations__research_1_1sat_1_1IntVar_1a2fdcfe89481f7f3e4cce763459764d78)
    (const [BoolVar](#_classoperations__research_1_1sat_1_1BoolVar)
    &var)

    Implicit cast
    [BoolVar](#_classoperations__research_1_1sat_1_1BoolVar) -\>
    [IntVar](#_classoperations__research_1_1sat_1_1IntVar).

-   [IntVar](#_classoperations__research_1_1sat_1_1IntVar)
    [WithName](#_classoperations__research_1_1sat_1_1IntVar_1a5f1761c6d2c5f7908f5f92bb16b91de9)
    (const std::string &name)

    Sets the name of the variable.

-   const std::string &
    [Name](#_classoperations__research_1_1sat_1_1IntVar_1aa8460c813c17ec5b7a137739c448bb98)
    () const

-   bool
    [operator==](#_classoperations__research_1_1sat_1_1IntVar_1a23d836e740ab297549905c5fa8539ba5)
    (const [IntVar](#_classoperations__research_1_1sat_1_1IntVar)
    &other) const

-   bool
    [operator!=](#_classoperations__research_1_1sat_1_1IntVar_1a824aadc0688ab57929ae744b1f1a7a26)
    (const [IntVar](#_classoperations__research_1_1sat_1_1IntVar)
    &other) const

-   std::string
    [DebugString](#_classoperations__research_1_1sat_1_1IntVar_1aac78f1c00b73fbad7bd6577181f537fb)
    () const

-   const IntegerVariableProto &
    [Proto](#_classoperations__research_1_1sat_1_1IntVar_1a426492195e6cdd88354def292ffa112f)
    () const

    Useful for testing.

-   IntegerVariableProto \*
    [MutableProto](#_classoperations__research_1_1sat_1_1IntVar_1a68349a30f6936d8f5a3d00d342ec5f3a)
    () const

    Useful for model edition.

-   int
    [index](#_classoperations__research_1_1sat_1_1IntVar_1ade91cda36a02fffbd115f1ec65746af1)
    () const

    Returns the index of the variable in the model.

<!-- -->

-   class
    [CpModelBuilder](#_classoperations__research_1_1sat_1_1IntVar_1ae04c85577cf33a05fb50bb361877fb42)

-   class
    [CumulativeConstraint](#_classoperations__research_1_1sat_1_1IntVar_1a9d31ad87d4edee55fc3cb5e239077720)

-   class
    [LinearExpr](#_classoperations__research_1_1sat_1_1IntVar_1a7678a938bf60a5c17fb47cf58995db0c)

-   class
    [IntervalVar](#_classoperations__research_1_1sat_1_1IntVar_1afc7f9983234a41167299a74f07ec6622)

-   class
    [ReservoirConstraint](#_classoperations__research_1_1sat_1_1IntVar_1ae0ff478f6506cb705bbc1737598276f4)

-   int64
    [SolutionIntegerValue](#_classoperations__research_1_1sat_1_1IntVar_1a64bd6fadf44a9840c837cc701b2b9043)
    (const CpSolverResponse &r, const
    [LinearExpr](#_classoperations__research_1_1sat_1_1LinearExpr)
    &expr)

    Evaluates the value of an linear expression in a solver response.

-   int64
    [SolutionIntegerMin](#_classoperations__research_1_1sat_1_1IntVar_1a8ec929aea42c9e50e2f1daf56525e379)
    (const CpSolverResponse &r,
    [IntVar](#_classoperations__research_1_1sat_1_1IntVar) x)

    Returns the min of an integer variable in a solution.

-   int64
    [SolutionIntegerMax](#_classoperations__research_1_1sat_1_1IntVar_1a79061f94ca7a97d0616f8b270358c771)
    (const CpSolverResponse &r,
    [IntVar](#_classoperations__research_1_1sat_1_1IntVar) x)

    Returns the max of an integer variable in a solution.

Detailed Description
--------------------

An integer variable. This class wraps an IntegerVariableProto. This can
only be constructed via
[CpModelBuilder.NewIntVar()](#_classoperations__research_1_1sat_1_1CpModelBuilder_1a0887a2fe4518bde7bbde18f592b6243f).

> **Note**
>
> a [BoolVar](#_classoperations__research_1_1sat_1_1BoolVar) can be used
> in any place that accept an
> [IntVar](#_classoperations__research_1_1sat_1_1IntVar) via an implicit
> cast. It will simply take the value 0 (when false) or 1 (when true).

Constructor & Destructor Documentation
--------------------------------------

### IntVar()`[1/2]`

IntVar

operations\_research::sat::IntVar

operations\_research::sat::IntVar

IntVar

`operations_research::sat::IntVar::IntVar ( )`

### IntVar()`[2/2]`

IntVar

operations\_research::sat::IntVar

operations\_research::sat::IntVar

IntVar

`operations_research::sat::IntVar::IntVar (const BoolVar & var)`

Implicit cast [BoolVar](#_classoperations__research_1_1sat_1_1BoolVar)
-\> [IntVar](#_classoperations__research_1_1sat_1_1IntVar).

Member Function Documentation
-----------------------------

### DebugString()

DebugString

operations\_research::sat::IntVar

operations\_research::sat::IntVar

DebugString

`std::string operations_research::sat::IntVar::DebugString ( ) const`

### index()

index

operations\_research::sat::IntVar

operations\_research::sat::IntVar

index

`int operations_research::sat::IntVar::index ( ) const[inline]`

Returns the index of the variable in the model.

### MutableProto()

MutableProto

operations\_research::sat::IntVar

operations\_research::sat::IntVar

MutableProto

`IntegerVariableProto* operations_research::sat::IntVar::MutableProto ( ) const[inline]`

Useful for model edition.

### Name()

Name

operations\_research::sat::IntVar

operations\_research::sat::IntVar

Name

`const std::string& operations_research::sat::IntVar::Name ( ) const[inline]`

### operator!=()

operator!=

operations\_research::sat::IntVar

operations\_research::sat::IntVar

operator!=

`bool operations_research::sat::IntVar::operator!= (const IntVar & other) const[inline]`

### operator==()

operator==

operations\_research::sat::IntVar

operations\_research::sat::IntVar

operator==

`bool operations_research::sat::IntVar::operator== (const IntVar & other) const[inline]`

### Proto()

Proto

operations\_research::sat::IntVar

operations\_research::sat::IntVar

Proto

`const IntegerVariableProto& operations_research::sat::IntVar::Proto ( ) const[inline]`

Useful for testing.

### WithName()

WithName

operations\_research::sat::IntVar

operations\_research::sat::IntVar

WithName

`IntVar operations_research::sat::IntVar::WithName (const std::string & name)`

Sets the name of the variable.

Friends And Related Function Documentation
------------------------------------------

### CpModelBuilder

CpModelBuilder

operations\_research::sat::IntVar

operations\_research::sat::IntVar

CpModelBuilder

`friend class CpModelBuilder[friend]`

### CumulativeConstraint

CumulativeConstraint

operations\_research::sat::IntVar

operations\_research::sat::IntVar

CumulativeConstraint

`friend class CumulativeConstraint[friend]`

### IntervalVar

IntervalVar

operations\_research::sat::IntVar

operations\_research::sat::IntVar

IntervalVar

`friend class IntervalVar[friend]`

### LinearExpr

LinearExpr

operations\_research::sat::IntVar

operations\_research::sat::IntVar

LinearExpr

`friend class LinearExpr[friend]`

### ReservoirConstraint

ReservoirConstraint

operations\_research::sat::IntVar

operations\_research::sat::IntVar

ReservoirConstraint

`friend class ReservoirConstraint[friend]`

### SolutionIntegerMax

SolutionIntegerMax

operations\_research::sat::IntVar

operations\_research::sat::IntVar

SolutionIntegerMax

`int64 SolutionIntegerMax (const CpSolverResponse & r, IntVar x)[friend]`

Returns the max of an integer variable in a solution.

### SolutionIntegerMin

SolutionIntegerMin

operations\_research::sat::IntVar

operations\_research::sat::IntVar

SolutionIntegerMin

`int64 SolutionIntegerMin (const CpSolverResponse & r, IntVar x)[friend]`

Returns the min of an integer variable in a solution.

### SolutionIntegerValue

SolutionIntegerValue

operations\_research::sat::IntVar

operations\_research::sat::IntVar

SolutionIntegerValue

`int64 SolutionIntegerValue (const CpSolverResponse & r, const LinearExpr & expr)[friend]`

Evaluates the value of an linear expression in a solver response.

The documentation for this class was generated from the following file:

ortools/sat/

cp\_model.h

operations\_research::sat::IntervalVar Class Reference {#_classoperations__research_1_1sat_1_1IntervalVar}
======================================================

operations\_research::sat::IntervalVar

`#include <cp_model.h>`

-   [IntervalVar](#_classoperations__research_1_1sat_1_1IntervalVar_1a106c293c6b0cac8589bc6b5b4ff0446c)
    ()

    Default ctor.

-   [IntervalVar](#_classoperations__research_1_1sat_1_1IntervalVar)
    [WithName](#_classoperations__research_1_1sat_1_1IntervalVar_1a1b7333dffeb56f1cffe35973cab19dd1)
    (const std::string &name)

    Sets the name of the variable.

-   std::string
    [Name](#_classoperations__research_1_1sat_1_1IntervalVar_1a48f98ff3c12aecf540170647a72ce860)
    () const

-   [IntVar](#_classoperations__research_1_1sat_1_1IntVar)
    [StartVar](#_classoperations__research_1_1sat_1_1IntervalVar_1a6228ce653636516ab2b2f760aa61a57e)
    () const

    Returns the start variable.

-   [IntVar](#_classoperations__research_1_1sat_1_1IntVar)
    [SizeVar](#_classoperations__research_1_1sat_1_1IntervalVar_1a9decc39f3f2079f78cdebd974972bc0f)
    () const

    Returns the size variable.

-   [IntVar](#_classoperations__research_1_1sat_1_1IntVar)
    [EndVar](#_classoperations__research_1_1sat_1_1IntervalVar_1ac86513192443e57e505b8e8c9ffb77f2)
    () const

    Returns the end variable.

-   [BoolVar](#_classoperations__research_1_1sat_1_1BoolVar)
    [PresenceBoolVar](#_classoperations__research_1_1sat_1_1IntervalVar_1aa5cc77b54d51bda6a6c8e30907b9a917)
    () const

-   bool
    [operator==](#_classoperations__research_1_1sat_1_1IntervalVar_1a34a66e31983270cb695c271d0b869ab3)
    (const
    [IntervalVar](#_classoperations__research_1_1sat_1_1IntervalVar)
    &other) const

-   bool
    [operator!=](#_classoperations__research_1_1sat_1_1IntervalVar_1a6816a7260a80aa691b7cc1e748323d21)
    (const
    [IntervalVar](#_classoperations__research_1_1sat_1_1IntervalVar)
    &other) const

-   std::string
    [DebugString](#_classoperations__research_1_1sat_1_1IntervalVar_1af90bf96ccc72778be5ebd9668e10d842)
    () const

-   const IntervalConstraintProto &
    [Proto](#_classoperations__research_1_1sat_1_1IntervalVar_1a34c3fc0d93697326a7e398cd45b1374d)
    () const

    Useful for testing.

-   IntervalConstraintProto \*
    [MutableProto](#_classoperations__research_1_1sat_1_1IntervalVar_1a4d10907c6da83ee20c29312f1064361f)
    () const

    Useful for model edition.

-   int
    [index](#_classoperations__research_1_1sat_1_1IntervalVar_1ac591e644d995d2520e859ee639695754)
    () const

    Returns the index of the interval constraint in the model.

<!-- -->

-   class
    [CpModelBuilder](#_classoperations__research_1_1sat_1_1IntervalVar_1ae04c85577cf33a05fb50bb361877fb42)

-   class
    [CumulativeConstraint](#_classoperations__research_1_1sat_1_1IntervalVar_1a9d31ad87d4edee55fc3cb5e239077720)

-   class
    [NoOverlap2DConstraint](#_classoperations__research_1_1sat_1_1IntervalVar_1abdbbe5d06195ef1dc4c30ad25b9017ac)

-   std::ostream &
    [operator\<\<](#_classoperations__research_1_1sat_1_1IntervalVar_1ad73e372cd9d1def69624f85777393123)
    (std::ostream &os, const
    [IntervalVar](#_classoperations__research_1_1sat_1_1IntervalVar)
    &var)

Detailed Description
--------------------

Represents a Interval variable.

An interval variable is both a constraint and a variable. It is defined
by three integer variables: start, size, and end.

It is a constraint because, internally, it enforces that start + size ==
end.

It is also a variable as it can appear in specific scheduling
constraints: NoOverlap, NoOverlap2D, Cumulative.

Optionally, a presence literal can be added to this constraint. This
presence literal is understood by the same constraints. These
constraints ignore interval variables with precence literals assigned to
false. Conversely, these constraints will also set these presence
literals to false if they cannot fit these intervals into the schedule.

It can only be constructed via
[CpModelBuilder.NewIntervalVar()](#_classoperations__research_1_1sat_1_1CpModelBuilder_1a4e1c85e161ee8e50f2f2162cd7294d03).

Constructor & Destructor Documentation
--------------------------------------

### IntervalVar()

IntervalVar

operations\_research::sat::IntervalVar

operations\_research::sat::IntervalVar

IntervalVar

`operations_research::sat::IntervalVar::IntervalVar ( )`

Default ctor.

Member Function Documentation
-----------------------------

### DebugString()

DebugString

operations\_research::sat::IntervalVar

operations\_research::sat::IntervalVar

DebugString

`std::string operations_research::sat::IntervalVar::DebugString ( ) const`

### EndVar()

EndVar

operations\_research::sat::IntervalVar

operations\_research::sat::IntervalVar

EndVar

`IntVar operations_research::sat::IntervalVar::EndVar ( ) const`

Returns the end variable.

### index()

index

operations\_research::sat::IntervalVar

operations\_research::sat::IntervalVar

index

`int operations_research::sat::IntervalVar::index ( ) const[inline]`

Returns the index of the interval constraint in the model.

### MutableProto()

MutableProto

operations\_research::sat::IntervalVar

operations\_research::sat::IntervalVar

MutableProto

`IntervalConstraintProto* operations_research::sat::IntervalVar::MutableProto ( ) const[inline]`

Useful for model edition.

### Name()

Name

operations\_research::sat::IntervalVar

operations\_research::sat::IntervalVar

Name

`std::string operations_research::sat::IntervalVar::Name ( ) const`

### operator!=()

operator!=

operations\_research::sat::IntervalVar

operations\_research::sat::IntervalVar

operator!=

`bool operations_research::sat::IntervalVar::operator!= (const IntervalVar & other) const[inline]`

### operator==()

operator==

operations\_research::sat::IntervalVar

operations\_research::sat::IntervalVar

operator==

`bool operations_research::sat::IntervalVar::operator== (const IntervalVar & other) const[inline]`

### PresenceBoolVar()

PresenceBoolVar

operations\_research::sat::IntervalVar

operations\_research::sat::IntervalVar

PresenceBoolVar

`BoolVar operations_research::sat::IntervalVar::PresenceBoolVar ( ) const`

Returns a [BoolVar](#_classoperations__research_1_1sat_1_1BoolVar)
indicating the presence of this interval. It returns
[CpModelBuilder.TrueVar()](#_classoperations__research_1_1sat_1_1CpModelBuilder_1a6dc655a67c5213fcefb82a213dac5e2c)
if the interval is not optional.

### Proto()

Proto

operations\_research::sat::IntervalVar

operations\_research::sat::IntervalVar

Proto

`const IntervalConstraintProto& operations_research::sat::IntervalVar::Proto ( ) const[inline]`

Useful for testing.

### SizeVar()

SizeVar

operations\_research::sat::IntervalVar

operations\_research::sat::IntervalVar

SizeVar

`IntVar operations_research::sat::IntervalVar::SizeVar ( ) const`

Returns the size variable.

### StartVar()

StartVar

operations\_research::sat::IntervalVar

operations\_research::sat::IntervalVar

StartVar

`IntVar operations_research::sat::IntervalVar::StartVar ( ) const`

Returns the start variable.

### WithName()

WithName

operations\_research::sat::IntervalVar

operations\_research::sat::IntervalVar

WithName

`IntervalVar operations_research::sat::IntervalVar::WithName (const std::string & name)`

Sets the name of the variable.

Friends And Related Function Documentation
------------------------------------------

### CpModelBuilder

CpModelBuilder

operations\_research::sat::IntervalVar

operations\_research::sat::IntervalVar

CpModelBuilder

`friend class CpModelBuilder[friend]`

### CumulativeConstraint

CumulativeConstraint

operations\_research::sat::IntervalVar

operations\_research::sat::IntervalVar

CumulativeConstraint

`friend class CumulativeConstraint[friend]`

### NoOverlap2DConstraint

NoOverlap2DConstraint

operations\_research::sat::IntervalVar

operations\_research::sat::IntervalVar

NoOverlap2DConstraint

`friend class NoOverlap2DConstraint[friend]`

### operator\<\<

operator\<\<

operations\_research::sat::IntervalVar

operations\_research::sat::IntervalVar

operator\<\<

`std::ostream& operator<< (std::ostream & os, const IntervalVar & var)[friend]`

The documentation for this class was generated from the following file:

ortools/sat/

cp\_model.h

operations\_research::sat::LinearExpr Class Reference {#_classoperations__research_1_1sat_1_1LinearExpr}
=====================================================

operations\_research::sat::LinearExpr

`#include <cp_model.h>`

-   [LinearExpr](#_classoperations__research_1_1sat_1_1LinearExpr_1ab33a810593c0a9f585133edcb22deb55)
    ()

-   [LinearExpr](#_classoperations__research_1_1sat_1_1LinearExpr_1aa9dc77d3e71adbebbebbaec9df2890e7)
    ([BoolVar](#_classoperations__research_1_1sat_1_1BoolVar) var)

-   [LinearExpr](#_classoperations__research_1_1sat_1_1LinearExpr_1abce3810df8307fd1d04f49b36a2e6693)
    ([IntVar](#_classoperations__research_1_1sat_1_1IntVar) var)

    Constructs a linear expression from an integer variable.

-   [LinearExpr](#_classoperations__research_1_1sat_1_1LinearExpr_1ab2654673474a3bdd47ddf4be52892292)
    (int64
    [constant](#_classoperations__research_1_1sat_1_1LinearExpr_1a050775df6d69660af8f78d577fd327cc))

    Constructs a constant linear expression.

-   [LinearExpr](#_classoperations__research_1_1sat_1_1LinearExpr) &
    [AddConstant](#_classoperations__research_1_1sat_1_1LinearExpr_1abed3c016b025d92058b1c29ddeef9341)
    (int64 value)

    Adds a constant value to the linear expression.

-   void
    [AddVar](#_classoperations__research_1_1sat_1_1LinearExpr_1afb9c31fb1176a9ba22d4b82fa285a5c7)
    ([IntVar](#_classoperations__research_1_1sat_1_1IntVar) var)

    Adds a single integer variable to the linear expression.

-   void
    [AddTerm](#_classoperations__research_1_1sat_1_1LinearExpr_1aa8bfd52517f0e1ca2a9adef474f1ff0c)
    ([IntVar](#_classoperations__research_1_1sat_1_1IntVar) var, int64
    coeff)

    Adds a term (var \* coeff) to the linear expression.

-   const std::vector\<
    [IntVar](#_classoperations__research_1_1sat_1_1IntVar) \> &
    [variables](#_classoperations__research_1_1sat_1_1LinearExpr_1af5805ba35a6efa9460c5d8eab8301172)
    () const

    Useful for testing.

-   const std::vector\< int64 \> &
    [coefficients](#_classoperations__research_1_1sat_1_1LinearExpr_1a6f0e8040bcb0ee633efd0862c660cbf4)
    () const

-   int64
    [constant](#_classoperations__research_1_1sat_1_1LinearExpr_1a050775df6d69660af8f78d577fd327cc)
    () const

<!-- -->

-   static
    [LinearExpr](#_classoperations__research_1_1sat_1_1LinearExpr)
    [Sum](#_classoperations__research_1_1sat_1_1LinearExpr_1a74026647307b38916135e8c3dad3421f)
    (absl::Span\< const
    [IntVar](#_classoperations__research_1_1sat_1_1IntVar) \> vars)

    Constructs the sum of a list of variables.

-   static
    [LinearExpr](#_classoperations__research_1_1sat_1_1LinearExpr)
    [ScalProd](#_classoperations__research_1_1sat_1_1LinearExpr_1a3b49fe9924ad61a609f65f4a7bc4c861)
    (absl::Span\< const
    [IntVar](#_classoperations__research_1_1sat_1_1IntVar) \> vars,
    absl::Span\< const int64 \> coeffs)

    Constructs the scalar product of variables and coefficients.

-   static
    [LinearExpr](#_classoperations__research_1_1sat_1_1LinearExpr)
    [BooleanSum](#_classoperations__research_1_1sat_1_1LinearExpr_1a0a6ff6ac94b7e556ff06df6f8211182f)
    (absl::Span\< const
    [BoolVar](#_classoperations__research_1_1sat_1_1BoolVar) \> vars)

    Constructs the sum of a list of Booleans.

-   static
    [LinearExpr](#_classoperations__research_1_1sat_1_1LinearExpr)
    [BooleanScalProd](#_classoperations__research_1_1sat_1_1LinearExpr_1ae62c3c0da3b623e5e43530c08f7bf379)
    (absl::Span\< const
    [BoolVar](#_classoperations__research_1_1sat_1_1BoolVar) \> vars,
    absl::Span\< const int64 \> coeffs)

    Constructs the scalar product of Booleans and coefficients.

Detailed Description
--------------------

A dedicated container for linear expressions.

This class helps building and manipulating linear expressions. With the
use of implicit constructors, it can accept integer values, Boolean and
Integer variables. Note that Not(x) will be silently transformed into 1
- x when added to the linear expression.

Furthermore, static methods allows sums and scalar products, with or
without an additional constant.

Usage:
[CpModelBuilder](#_classoperations__research_1_1sat_1_1CpModelBuilder)
cp\_model; [IntVar](#_classoperations__research_1_1sat_1_1IntVar) x =
model.NewIntVar(0, 10, \"x\");
[IntVar](#_classoperations__research_1_1sat_1_1IntVar) y =
model.NewIntVar(0, 10, \"y\");
[BoolVar](#_classoperations__research_1_1sat_1_1BoolVar) b =
model.NewBoolVar().WithName(\"b\");
[BoolVar](#_classoperations__research_1_1sat_1_1BoolVar) c =
model.NewBoolVar().WithName(\"c\");
[LinearExpr](#_classoperations__research_1_1sat_1_1LinearExpr) e1(x);
///\< e1 = x.
[LinearExpr](#_classoperations__research_1_1sat_1_1LinearExpr) e2 =
[LinearExpr::Sum](#_classoperations__research_1_1sat_1_1LinearExpr_1a74026647307b38916135e8c3dad3421f)({x,
y}).AddConstant(5); ///\< e2 = x + y + 5;
[LinearExpr](#_classoperations__research_1_1sat_1_1LinearExpr) e3 =
[LinearExpr::ScalProd](#_classoperations__research_1_1sat_1_1LinearExpr_1a3b49fe9924ad61a609f65f4a7bc4c861)({x,
y}, {2, -1}); ///\< e3 = 2 \* x - y.
[LinearExpr](#_classoperations__research_1_1sat_1_1LinearExpr) e4(b);
///\< e4 = b.
[LinearExpr](#_classoperations__research_1_1sat_1_1LinearExpr)
e5([b.Not()](#_namespaceoperations__research_1_1sat_1a5e3de118c1f8dd5a7ec21704e05684b9));
///\< e5 = 1 - b. ///\< If passing a std::vector\<BoolVar\>, a
specialized method must be called. std::vector\<BoolVar\> bools = {b,
Not(c)}; [LinearExpr](#_classoperations__research_1_1sat_1_1LinearExpr)
e6 = LinearExpr::BooleanSum(bools); ///\< e6 = b + 1 - c; ///\< e7 = -3
\* b + 1 - c;
[LinearExpr](#_classoperations__research_1_1sat_1_1LinearExpr) e7 =
[LinearExpr::BooleanScalProd](#_classoperations__research_1_1sat_1_1LinearExpr_1ae62c3c0da3b623e5e43530c08f7bf379)(bools,
{-3, 1});

This can be used implicitly in some of the
[CpModelBuilder](#_classoperations__research_1_1sat_1_1CpModelBuilder)
methods. cp\_model.AddGreaterThan(x, 5); ///\< x \> 5
cp\_model.AddEquality(x, LinearExpr(y).AddConstant(5)); ///\< x == y + 5

Constructor & Destructor Documentation
--------------------------------------

### LinearExpr()`[1/4]`

LinearExpr

operations\_research::sat::LinearExpr

operations\_research::sat::LinearExpr

LinearExpr

`operations_research::sat::LinearExpr::LinearExpr ( )`

### LinearExpr()`[2/4]`

LinearExpr

operations\_research::sat::LinearExpr

operations\_research::sat::LinearExpr

LinearExpr

`operations_research::sat::LinearExpr::LinearExpr (BoolVar var)`

Constructs a linear expression from a Boolean variable. It deals with
logical negation correctly.

### LinearExpr()`[3/4]`

LinearExpr

operations\_research::sat::LinearExpr

operations\_research::sat::LinearExpr

LinearExpr

`operations_research::sat::LinearExpr::LinearExpr (IntVar var)`

Constructs a linear expression from an integer variable.

### LinearExpr()`[4/4]`

LinearExpr

operations\_research::sat::LinearExpr

operations\_research::sat::LinearExpr

LinearExpr

`operations_research::sat::LinearExpr::LinearExpr (int64 constant)`

Constructs a constant linear expression.

Member Function Documentation
-----------------------------

### AddConstant()

AddConstant

operations\_research::sat::LinearExpr

operations\_research::sat::LinearExpr

AddConstant

`LinearExpr& operations_research::sat::LinearExpr::AddConstant (int64 value)`

Adds a constant value to the linear expression.

### AddTerm()

AddTerm

operations\_research::sat::LinearExpr

operations\_research::sat::LinearExpr

AddTerm

`void operations_research::sat::LinearExpr::AddTerm (IntVar var, int64 coeff)`

Adds a term (var \* coeff) to the linear expression.

### AddVar()

AddVar

operations\_research::sat::LinearExpr

operations\_research::sat::LinearExpr

AddVar

`void operations_research::sat::LinearExpr::AddVar (IntVar var)`

Adds a single integer variable to the linear expression.

### BooleanScalProd()

BooleanScalProd

operations\_research::sat::LinearExpr

operations\_research::sat::LinearExpr

BooleanScalProd

`static LinearExpr operations_research::sat::LinearExpr::BooleanScalProd (absl::Span< const BoolVar > vars, absl::Span< const int64 > coeffs)[static]`

Constructs the scalar product of Booleans and coefficients.

### BooleanSum()

BooleanSum

operations\_research::sat::LinearExpr

operations\_research::sat::LinearExpr

BooleanSum

`static LinearExpr operations_research::sat::LinearExpr::BooleanSum (absl::Span< const BoolVar > vars)[static]`

Constructs the sum of a list of Booleans.

### coefficients()

coefficients

operations\_research::sat::LinearExpr

operations\_research::sat::LinearExpr

coefficients

`const std::vector<int64>& operations_research::sat::LinearExpr::coefficients ( ) const[inline]`

### constant()

constant

operations\_research::sat::LinearExpr

operations\_research::sat::LinearExpr

constant

`int64 operations_research::sat::LinearExpr::constant ( ) const[inline]`

### ScalProd()

ScalProd

operations\_research::sat::LinearExpr

operations\_research::sat::LinearExpr

ScalProd

`static LinearExpr operations_research::sat::LinearExpr::ScalProd (absl::Span< const IntVar > vars, absl::Span< const int64 > coeffs)[static]`

Constructs the scalar product of variables and coefficients.

### Sum()

Sum

operations\_research::sat::LinearExpr

operations\_research::sat::LinearExpr

Sum

`static LinearExpr operations_research::sat::LinearExpr::Sum (absl::Span< const IntVar > vars)[static]`

Constructs the sum of a list of variables.

### variables()

variables

operations\_research::sat::LinearExpr

operations\_research::sat::LinearExpr

variables

`const std::vector<IntVar>& operations_research::sat::LinearExpr::variables ( ) const[inline]`

Useful for testing.

The documentation for this class was generated from the following file:

ortools/sat/

cp\_model.h

operations\_research::sat::Model Class Reference {#_classoperations__research_1_1sat_1_1Model}
================================================

operations\_research::sat::Model

`#include <model.h>`

-   [Model](#_classoperations__research_1_1sat_1_1Model_1a7d97534275c629f2917ed5a029e2e2c5)
    ()

-   template\<typename T \>

    T
    [Add](#_classoperations__research_1_1sat_1_1Model_1a059b9d223761f2b9cc82df4871ae36fa)
    (std::function\<
    T([Model](#_classoperations__research_1_1sat_1_1Model) \*)\> f)

-   template\<typename T \>

    T
    [Get](#_classoperations__research_1_1sat_1_1Model_1a162bdd37a619fd04f9085005008951d9)
    (std::function\< T(const
    [Model](#_classoperations__research_1_1sat_1_1Model) &)\> f) const

    Similar to
    [Add()](#_classoperations__research_1_1sat_1_1Model_1a059b9d223761f2b9cc82df4871ae36fa)
    but this is const.

-   template\<typename T \>

    T \*
    [GetOrCreate](#_classoperations__research_1_1sat_1_1Model_1af4eb422b7cfd963c58d65b18b4e47717)
    ()

-   template\<typename T \>

    const T \*
    [Get](#_classoperations__research_1_1sat_1_1Model_1a211cf867e9edd220616c0a8f6ba4b71d)
    () const

-   template\<typename T \>

    T \*
    [Mutable](#_classoperations__research_1_1sat_1_1Model_1a6226de9875c58b81f461c123577d1689)
    () const

    Same as
    [Get()](#_classoperations__research_1_1sat_1_1Model_1a162bdd37a619fd04f9085005008951d9),
    but returns a mutable version of the object.

-   template\<typename T \>

    void
    [TakeOwnership](#_classoperations__research_1_1sat_1_1Model_1a6411a892fd57781615c9edf80081026c)
    (T \*t)

-   template\<typename T \>

    T \*
    [Create](#_classoperations__research_1_1sat_1_1Model_1a08f2ee3dc9fa03be18a4c38304e068d9)
    ()

-   template\<typename T \>

    void
    [Register](#_classoperations__research_1_1sat_1_1Model_1a78f476ca154e64d281ae07efd825a779)
    (T \*non\_owned\_class)

Detailed Description
--------------------

Class that owns everything related to a particular optimization model.

This class is actually a fully generic wrapper that can hold any type of
constraints, watchers, solvers and provide a mecanism to wire them
together.

Constructor & Destructor Documentation
--------------------------------------

### Model()

Model

operations\_research::sat::Model

operations\_research::sat::Model

Model

`operations_research::sat::Model::Model ( )[inline]`

Member Function Documentation
-----------------------------

### Add()

Add

operations\_research::sat::Model

operations\_research::sat::Model

Add

template\<typename T \>

`T operations_research::sat::Model::Add (std::function< T(Model *)> f)[inline]`

This allows to have a nicer API on the client side, and it allows both
of these forms:

-   ConstraintCreationFunction(contraint\_args, &model);

-   model.Add(ConstraintCreationFunction(contraint\_args));

The second form is a bit nicer for the client and it also allows to
store constraints and add them later. However, the function creating the
constraint is slighly more involved:

std::function\<void(Model\*)\>
ConstraintCreationFunction(contraint\_args) { return \[=\] (Model\*
model) { \... the same code \... }; }

We also have a templated return value for the functions that need it
like const BooleanVariable b = model.Add(NewBooleanVariable()); const
IntegerVariable i = model.Add(NewWeightedSum(weigths, variables));

### Create()

Create

operations\_research::sat::Model

operations\_research::sat::Model

Create

template\<typename T \>

`T* operations_research::sat::Model::Create ( )[inline]`

This returns a non-singleton object owned by the model and created with
the T(Model\* model) constructor if it exist or the T() constructor
otherwise. It is just a shortcut to new +
[TakeOwnership()](#_classoperations__research_1_1sat_1_1Model_1a6411a892fd57781615c9edf80081026c).

### Get()`[1/2]`

Get

operations\_research::sat::Model

operations\_research::sat::Model

Get

template\<typename T \>

`T operations_research::sat::Model::Get (std::function< T(const Model &)> f) const[inline]`

Similar to
[Add()](#_classoperations__research_1_1sat_1_1Model_1a059b9d223761f2b9cc82df4871ae36fa)
but this is const.

### Get()`[2/2]`

Get

operations\_research::sat::Model

operations\_research::sat::Model

Get

template\<typename T \>

`const T* operations_research::sat::Model::Get ( ) const[inline]`

Likes
[GetOrCreate()](#_classoperations__research_1_1sat_1_1Model_1af4eb422b7cfd963c58d65b18b4e47717)
but do not create the object if it is non-existing. This returns a const
version of the object.

### GetOrCreate()

GetOrCreate

operations\_research::sat::Model

operations\_research::sat::Model

GetOrCreate

template\<typename T \>

`T* operations_research::sat::Model::GetOrCreate ( )[inline]`

Returns an object of type T that is unique to this model (like a
\"local\" singleton). This returns an already created instance or create
a new one if needed using the T(Model\* model) constructor if it exist
or T() otherwise.

This works a bit like in a dependency injection framework and allows to
really easily wire all the classes that make up a solver together. For
instance a constraint can depends on the LiteralTrail, or the
IntegerTrail or both, it can depend on a Watcher class to register
itself in order to be called when needed and so on.

IMPORTANT: the Model\* constructors function shouldn\'t form a cycle
between each other, otherwise this will crash the program.

[Todo](#_todo_1_todo000003)

(user): directly store std::unique\_ptr\<\> in singletons\_?

### Mutable()

Mutable

operations\_research::sat::Model

operations\_research::sat::Model

Mutable

template\<typename T \>

`T* operations_research::sat::Model::Mutable ( ) const[inline]`

Same as
[Get()](#_classoperations__research_1_1sat_1_1Model_1a162bdd37a619fd04f9085005008951d9),
but returns a mutable version of the object.

### Register()

Register

operations\_research::sat::Model

operations\_research::sat::Model

Register

template\<typename T \>

`void operations_research::sat::Model::Register (T * non_owned_class)[inline]`

Register a non-owned class that will be \"singleton\" in the model. It
is an error to call this on an already registered class.

### TakeOwnership()

TakeOwnership

operations\_research::sat::Model

operations\_research::sat::Model

TakeOwnership

template\<typename T \>

`void operations_research::sat::Model::TakeOwnership (T * t)[inline]`

Gives ownership of a pointer to this model. It will be destroyed when
the model is.

The documentation for this class was generated from the following file:

ortools/sat/

model.h

operations\_research::sat::NoOverlap2DConstraint Class Reference {#_classoperations__research_1_1sat_1_1NoOverlap2DConstraint}
================================================================

operations\_research::sat::NoOverlap2DConstraint

`#include <cp_model.h>`

Inheritance diagram for
operations\_research::sat::NoOverlap2DConstraint:

Collaboration diagram for
operations\_research::sat::NoOverlap2DConstraint:

-   void
    [AddRectangle](#_classoperations__research_1_1sat_1_1NoOverlap2DConstraint_1a7e76dae6971e2f38651b7eb8411ebe63)
    ([IntervalVar](#_classoperations__research_1_1sat_1_1IntervalVar)
    x\_coordinate,
    [IntervalVar](#_classoperations__research_1_1sat_1_1IntervalVar)
    y\_coordinate)

<!-- -->

-   class
    [CpModelBuilder](#_classoperations__research_1_1sat_1_1NoOverlap2DConstraint_1ae04c85577cf33a05fb50bb361877fb42)

Detailed Description
--------------------

Specialized no\_overlap2D constraint.

This constraint allows adding rectangles to the no\_overlap2D constraint
incrementally.

Member Function Documentation
-----------------------------

### AddRectangle()

AddRectangle

operations\_research::sat::NoOverlap2DConstraint

operations\_research::sat::NoOverlap2DConstraint

AddRectangle

`void operations_research::sat::NoOverlap2DConstraint::AddRectangle (IntervalVar x_coordinate, IntervalVar y_coordinate)`

Friends And Related Function Documentation
------------------------------------------

### CpModelBuilder

CpModelBuilder

operations\_research::sat::NoOverlap2DConstraint

operations\_research::sat::NoOverlap2DConstraint

CpModelBuilder

`friend class CpModelBuilder[friend]`

The documentation for this class was generated from the following file:

ortools/sat/

cp\_model.h

operations\_research::sat::ReservoirConstraint Class Reference {#_classoperations__research_1_1sat_1_1ReservoirConstraint}
==============================================================

operations\_research::sat::ReservoirConstraint

`#include <cp_model.h>`

Inheritance diagram for operations\_research::sat::ReservoirConstraint:

Collaboration diagram for
operations\_research::sat::ReservoirConstraint:

-   void
    [AddEvent](#_classoperations__research_1_1sat_1_1ReservoirConstraint_1aff0e9a5c156c176def60cf2985919bd6)
    ([IntVar](#_classoperations__research_1_1sat_1_1IntVar) time, int64
    demand)

-   void
    [AddOptionalEvent](#_classoperations__research_1_1sat_1_1ReservoirConstraint_1aad9028f0c33c7d4799891b9f742148b6)
    ([IntVar](#_classoperations__research_1_1sat_1_1IntVar) time, int64
    demand, [BoolVar](#_classoperations__research_1_1sat_1_1BoolVar)
    is\_active)

<!-- -->

-   class
    [CpModelBuilder](#_classoperations__research_1_1sat_1_1ReservoirConstraint_1ae04c85577cf33a05fb50bb361877fb42)

Detailed Description
--------------------

Specialized reservoir constraint.

This constraint allows adding emptying/refilling events to the reservoir
constraint incrementally.

Member Function Documentation
-----------------------------

### AddEvent()

AddEvent

operations\_research::sat::ReservoirConstraint

operations\_research::sat::ReservoirConstraint

AddEvent

`void operations_research::sat::ReservoirConstraint::AddEvent (IntVar time, int64 demand)`

### AddOptionalEvent()

AddOptionalEvent

operations\_research::sat::ReservoirConstraint

operations\_research::sat::ReservoirConstraint

AddOptionalEvent

`void operations_research::sat::ReservoirConstraint::AddOptionalEvent (IntVar time, int64 demand, BoolVar is_active)`

Friends And Related Function Documentation
------------------------------------------

### CpModelBuilder

CpModelBuilder

operations\_research::sat::ReservoirConstraint

operations\_research::sat::ReservoirConstraint

CpModelBuilder

`friend class CpModelBuilder[friend]`

The documentation for this class was generated from the following file:

ortools/sat/

cp\_model.h

operations\_research::sat::TableConstraint Class Reference {#_classoperations__research_1_1sat_1_1TableConstraint}
==========================================================

operations\_research::sat::TableConstraint

`#include <cp_model.h>`

Inheritance diagram for operations\_research::sat::TableConstraint:

Collaboration diagram for operations\_research::sat::TableConstraint:

-   void
    [AddTuple](#_classoperations__research_1_1sat_1_1TableConstraint_1a90017a38e8ac8eaf4644bdce5e5e1420)
    (absl::Span\< const int64 \> tuple)

<!-- -->

-   class
    [CpModelBuilder](#_classoperations__research_1_1sat_1_1TableConstraint_1ae04c85577cf33a05fb50bb361877fb42)

Detailed Description
--------------------

Specialized assignment constraint.

This constraint allows adding tuples to the allowed/forbidden assignment
constraint incrementally.

Member Function Documentation
-----------------------------

### AddTuple()

AddTuple

operations\_research::sat::TableConstraint

operations\_research::sat::TableConstraint

AddTuple

`void operations_research::sat::TableConstraint::AddTuple (absl::Span< const int64 > tuple)`

Friends And Related Function Documentation
------------------------------------------

### CpModelBuilder

CpModelBuilder

operations\_research::sat::TableConstraint

operations\_research::sat::TableConstraint

CpModelBuilder

`friend class CpModelBuilder[friend]`

The documentation for this class was generated from the following file:

ortools/sat/

cp\_model.h

ortools/sat/cp\_model.h File Reference {#_cp__model_8h}
======================================

ortools/sat/cp\_model.h

    #include <string>
    #include "absl/container/flat_hash_map.h"
    #include "absl/types/span.h"
    #include "ortools/sat/cp_model.pb.h"
    #include "ortools/sat/cp_model_solver.h"
    #include "ortools/sat/cp_model_utils.h"
    #include "ortools/sat/model.h"
    #include "ortools/sat/sat_parameters.pb.h"
    #include "ortools/util/sorted_interval_list.h"

Include dependency graph for cp\_model.h:

-   class
    [operations\_research::sat::BoolVar](#_classoperations__research_1_1sat_1_1BoolVar)

-   class
    [operations\_research::sat::IntVar](#_classoperations__research_1_1sat_1_1IntVar)

-   class
    [operations\_research::sat::LinearExpr](#_classoperations__research_1_1sat_1_1LinearExpr)

-   class
    [operations\_research::sat::IntervalVar](#_classoperations__research_1_1sat_1_1IntervalVar)

-   class
    [operations\_research::sat::Constraint](#_classoperations__research_1_1sat_1_1Constraint)

-   class
    [operations\_research::sat::CircuitConstraint](#_classoperations__research_1_1sat_1_1CircuitConstraint)

-   class
    [operations\_research::sat::TableConstraint](#_classoperations__research_1_1sat_1_1TableConstraint)

-   class
    [operations\_research::sat::ReservoirConstraint](#_classoperations__research_1_1sat_1_1ReservoirConstraint)

-   class
    [operations\_research::sat::AutomatonConstraint](#_classoperations__research_1_1sat_1_1AutomatonConstraint)

-   class
    [operations\_research::sat::NoOverlap2DConstraint](#_classoperations__research_1_1sat_1_1NoOverlap2DConstraint)

-   class
    [operations\_research::sat::CumulativeConstraint](#_classoperations__research_1_1sat_1_1CumulativeConstraint)

-   class
    [operations\_research::sat::CpModelBuilder](#_classoperations__research_1_1sat_1_1CpModelBuilder)

<!-- -->

-   [operations\_research](#_namespaceoperations__research)

-   [operations\_research::sat](#_namespaceoperations__research_1_1sat)

<!-- -->

-   std::ostream &
    [operations\_research::sat::operator\<\<](#_namespaceoperations__research_1_1sat_1a9c0ae0d048a431656985fc79428bbe67)
    (std::ostream &os, const BoolVar &var)

-   BoolVar
    [operations\_research::sat::Not](#_namespaceoperations__research_1_1sat_1a5e3de118c1f8dd5a7ec21704e05684b9)
    (BoolVar x)

-   std::ostream &
    [operations\_research::sat::operator\<\<](#_namespaceoperations__research_1_1sat_1a57b8aabbc5b3c1d177d35b3ebcf9b5fa)
    (std::ostream &os, const IntVar &var)

-   std::ostream &
    [operations\_research::sat::operator\<\<](#_namespaceoperations__research_1_1sat_1ae9f86b31794751c624a783d15306280c)
    (std::ostream &os, const IntervalVar &var)

-   int64
    [operations\_research::sat::SolutionIntegerValue](#_namespaceoperations__research_1_1sat_1aeaed9bdf2a27bb778ba397666cb874d7)
    (const CpSolverResponse &r, const LinearExpr &expr)

    Evaluates the value of an linear expression in a solver response.

-   int64
    [operations\_research::sat::SolutionIntegerMin](#_namespaceoperations__research_1_1sat_1a671200a31003492dbef21f2b4ee3dcbd)
    (const CpSolverResponse &r, IntVar x)

    Returns the min of an integer variable in a solution.

-   int64
    [operations\_research::sat::SolutionIntegerMax](#_namespaceoperations__research_1_1sat_1a8ec893fa736de5b95135ecb9314ee6d8)
    (const CpSolverResponse &r, IntVar x)

    Returns the max of an integer variable in a solution.

-   bool
    [operations\_research::sat::SolutionBooleanValue](#_namespaceoperations__research_1_1sat_1afa415e372a9d64eede869ed98666c29c)
    (const CpSolverResponse &r, BoolVar x)

ortools/sat/cp\_model\_solver.h File Reference {#_cp__model__solver_8h}
==============================================

ortools/sat/cp\_model\_solver.h

    #include <functional>
    #include <string>
    #include <vector>
    #include "ortools/base/integral_types.h"
    #include "ortools/sat/cp_model.pb.h"
    #include "ortools/sat/model.h"
    #include "ortools/sat/sat_parameters.pb.h"

Include dependency graph for cp\_model\_solver.h:

This graph shows which files directly or indirectly include this file:

-   [operations\_research](#_namespaceoperations__research)

-   [operations\_research::sat](#_namespaceoperations__research_1_1sat)

<!-- -->

-   std::string
    [operations\_research::sat::CpModelStats](#_namespaceoperations__research_1_1sat_1a287579e5f181fc7c89feccf1128faffb)
    (const CpModelProto &model)

    Returns a std::string with some statistics on the given
    CpModelProto.

-   std::string
    [operations\_research::sat::CpSolverResponseStats](#_namespaceoperations__research_1_1sat_1ac2d87e8109f9c60f7af84a60106abd57)
    (const CpSolverResponse &response)

    Returns a std::string with some statistics on the solver response.

-   CpSolverResponse
    [operations\_research::sat::SolveCpModel](#_namespaceoperations__research_1_1sat_1a9d67b9c66f1cb9c1dcc3415cd5af11bf)
    (const CpModelProto &model\_proto, Model \*model)

-   CpSolverResponse
    [operations\_research::sat::Solve](#_namespaceoperations__research_1_1sat_1a09d851f944ab4f305c3d9f8df99b7bf8)
    (const CpModelProto &model\_proto)

    Solves the given cp\_model and returns an instance of
    CpSolverResponse.

-   CpSolverResponse
    [operations\_research::sat::SolveWithParameters](#_namespaceoperations__research_1_1sat_1aa3062797aa0396abf37dbcc99a746f12)
    (const CpModelProto &model\_proto, const SatParameters &params)

-   CpSolverResponse
    [operations\_research::sat::SolveWithParameters](#_namespaceoperations__research_1_1sat_1af52c27ecb43d6486c1a70e022b4aad39)
    (const CpModelProto &model\_proto, const std::string &params)

-   void
    [operations\_research::sat::SetSynchronizationFunction](#_namespaceoperations__research_1_1sat_1ad04337634227eac006d3e33a7028f82f)
    (std::function\< CpSolverResponse()\> f, Model \*model)

<!-- -->

-   std::function\< void(Model \*)\>
    [operations\_research::sat::NewFeasibleSolutionObserver](#_namespaceoperations__research_1_1sat_1aacf15d440f0db4cd0a63c8aebe85db6d)
    (const std::function\< void(const CpSolverResponse &response)\>
    &observer)

-   std::function\< SatParameters(Model \*)\>
    [operations\_research::sat::NewSatParameters](#_namespaceoperations__research_1_1sat_1a10700832ca6bc420f2931eb707957b0b)
    (const std::string &params)

 {#_deprecated}

Member [operations\_research::Domain::intervals](#_classoperations__research_1_1Domain_1a153837b45a0e9ddf5620679f243baa96) () const

:   

ortools/util Directory Reference {#_dir_a3328a0ea67a2aaa160c2783ffbaa5dc}
================================

ortools/util Directory Reference

-   file [sorted\_interval\_list.h](#_sorted__interval__list_8h)

ortools Directory Reference {#_dir_a7cc1eeded8f693d0da6c729bc88c45a}
===========================

ortools Directory Reference

Directory dependency graph for ortools:

-   directory [sat](#_dir_dddac007a45022d9da6ea1dee012c3b9)

-   directory [util](#_dir_a3328a0ea67a2aaa160c2783ffbaa5dc)

ortools/sat Directory Reference {#_dir_dddac007a45022d9da6ea1dee012c3b9}
===============================

ortools/sat Directory Reference

Directory dependency graph for sat:

-   file [cp\_model.h](#_cp__model_8h)

-   file [cp\_model\_solver.h](#_cp__model__solver_8h)

-   file [model.h](#_model_8h)

Todo List
=========

Deprecated List
===============

Namespace Documentation
=======================

Class Documentation
===================

File Documentation
==================

ortools/sat/model.h File Reference {#_model_8h}
==================================

ortools/sat/model.h

    #include <cstddef>
    #include <functional>
    #include <map>
    #include <memory>
    #include <vector>
    #include "ortools/base/logging.h"
    #include "ortools/base/macros.h"
    #include "ortools/base/map_util.h"
    #include "ortools/base/typeid.h"

Include dependency graph for model.h:

This graph shows which files directly or indirectly include this file:

-   class
    [operations\_research::sat::Model](#_classoperations__research_1_1sat_1_1Model)

<!-- -->

-   [operations\_research](#_namespaceoperations__research)

-   [operations\_research::sat](#_namespaceoperations__research_1_1sat)

operations\_research Namespace Reference {#_namespaceoperations__research}
========================================

operations\_research

-   [sat](#_namespaceoperations__research_1_1sat)

<!-- -->

-   struct
    [ClosedInterval](#_structoperations__research_1_1ClosedInterval)

    Represents a closed interval \[start, end\]. We must have start \<=
    end.

-   class [Domain](#_classoperations__research_1_1Domain)

-   class
    [SortedDisjointIntervalList](#_classoperations__research_1_1SortedDisjointIntervalList)

<!-- -->

-   std::ostream &
    [operator\<\<](#_namespaceoperations__research_1a5c341d9214d5d46014089435ba0e26d3)
    (std::ostream &out, const
    [ClosedInterval](#_structoperations__research_1_1ClosedInterval)
    &interval)

-   std::ostream &
    [operator\<\<](#_namespaceoperations__research_1aaa301d39d2a9271daf8c65e779635335)
    (std::ostream &out, const std::vector\<
    [ClosedInterval](#_structoperations__research_1_1ClosedInterval) \>
    &intervals)

-   bool
    [IntervalsAreSortedAndNonAdjacent](#_namespaceoperations__research_1ab8c23924c4b61ed5c531424a6f18bde1)
    (absl::Span\< const
    [ClosedInterval](#_structoperations__research_1_1ClosedInterval) \>
    intervals)

-   std::ostream &
    [operator\<\<](#_namespaceoperations__research_1abebf3070a940da6bf678953a66584e76)
    (std::ostream &out, const
    [Domain](#_classoperations__research_1_1Domain) &domain)

Detailed Description
--------------------

Licensed under the Apache License, Version 2.0 (the \"License\"); you
may not use this file except in compliance with the License. You may
obtain a copy of the License at
`http://www.apache.org/licenses/LICENSE-2.0
` Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an \"AS IS\" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License. This file implements a wrapper around the
CP-SAT model proto.

Here is a minimal example that shows how to create a model, solve it,
and print out the solution.

CpModelBuilder cp\_model;
[Domain](#_classoperations__research_1_1Domain) all\_animals(0, 20);
IntVar rabbits =
cp\_model.NewIntVar(all\_animals).WithName(\"rabbits\"); IntVar
pheasants = cp\_model.NewIntVar(all\_animals).WithName(\"pheasants\");

cp\_model.AddEquality(LinearExpr::Sum({rabbits, pheasants}), 20);
cp\_model.AddEquality(LinearExpr::ScalProd({rabbits, pheasants}, {4,
2}), 56);

const CpSolverResponse response = Solve(cp\_model); if
(response.status() == CpSolverStatus::FEASIBLE) { LOG(INFO) \<\<
SolutionIntegerValue(response, rabbits) \<\< \" rabbits, and \" \<\<
SolutionIntegerValue(response, pheasants) \<\< \" pheasants.\"; }

Licensed under the Apache License, Version 2.0 (the \"License\"); you
may not use this file except in compliance with the License. You may
obtain a copy of the License at
`http://www.apache.org/licenses/LICENSE-2.0
` Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an \"AS IS\" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.

Function Documentation
----------------------

### IntervalsAreSortedAndNonAdjacent()

IntervalsAreSortedAndNonAdjacent

operations\_research

operations\_research

IntervalsAreSortedAndNonAdjacent

`bool operations_research::IntervalsAreSortedAndNonAdjacent (absl::Span< const ClosedInterval > intervals)`

Returns true iff we have:

-   The intervals appear in increasing order.

-   for all i: intervals\[i\].start \<= intervals\[i\].end

-   for all i but the last: intervals\[i\].end + 1 \<
    intervals\[i+1\].start

### operator\<\<()`[1/3]`

operator\<\<

operations\_research

operations\_research

operator\<\<

`std::ostream& operations_research::operator<< (std::ostream & out, const ClosedInterval & interval)`

### operator\<\<()`[2/3]`

operator\<\<

operations\_research

operations\_research

operator\<\<

`std::ostream& operations_research::operator<< (std::ostream & out, const std::vector< ClosedInterval > & intervals)`

### operator\<\<()`[3/3]`

operator\<\<

operations\_research

operations\_research

operator\<\<

`std::ostream& operations_research::operator<< (std::ostream & out, const Domain & domain)`

operations\_research::sat Namespace Reference {#_namespaceoperations__research_1_1sat}
=============================================

operations\_research::sat

-   class
    [AutomatonConstraint](#_classoperations__research_1_1sat_1_1AutomatonConstraint)

-   class [BoolVar](#_classoperations__research_1_1sat_1_1BoolVar)

-   class
    [CircuitConstraint](#_classoperations__research_1_1sat_1_1CircuitConstraint)

-   class [Constraint](#_classoperations__research_1_1sat_1_1Constraint)

-   class
    [CpModelBuilder](#_classoperations__research_1_1sat_1_1CpModelBuilder)

-   class
    [CumulativeConstraint](#_classoperations__research_1_1sat_1_1CumulativeConstraint)

-   class
    [IntervalVar](#_classoperations__research_1_1sat_1_1IntervalVar)

-   class [IntVar](#_classoperations__research_1_1sat_1_1IntVar)

-   class [LinearExpr](#_classoperations__research_1_1sat_1_1LinearExpr)

-   class [Model](#_classoperations__research_1_1sat_1_1Model)

-   class
    [NoOverlap2DConstraint](#_classoperations__research_1_1sat_1_1NoOverlap2DConstraint)

-   class
    [ReservoirConstraint](#_classoperations__research_1_1sat_1_1ReservoirConstraint)

-   class
    [TableConstraint](#_classoperations__research_1_1sat_1_1TableConstraint)

<!-- -->

-   std::ostream &
    [operator\<\<](#_namespaceoperations__research_1_1sat_1a9c0ae0d048a431656985fc79428bbe67)
    (std::ostream &os, const
    [BoolVar](#_classoperations__research_1_1sat_1_1BoolVar) &var)

-   [BoolVar](#_classoperations__research_1_1sat_1_1BoolVar)
    [Not](#_namespaceoperations__research_1_1sat_1a5e3de118c1f8dd5a7ec21704e05684b9)
    ([BoolVar](#_classoperations__research_1_1sat_1_1BoolVar) x)

-   std::ostream &
    [operator\<\<](#_namespaceoperations__research_1_1sat_1a57b8aabbc5b3c1d177d35b3ebcf9b5fa)
    (std::ostream &os, const
    [IntVar](#_classoperations__research_1_1sat_1_1IntVar) &var)

-   std::ostream &
    [operator\<\<](#_namespaceoperations__research_1_1sat_1ae9f86b31794751c624a783d15306280c)
    (std::ostream &os, const
    [IntervalVar](#_classoperations__research_1_1sat_1_1IntervalVar)
    &var)

-   int64
    [SolutionIntegerValue](#_namespaceoperations__research_1_1sat_1aeaed9bdf2a27bb778ba397666cb874d7)
    (const CpSolverResponse &r, const
    [LinearExpr](#_classoperations__research_1_1sat_1_1LinearExpr)
    &expr)

    Evaluates the value of an linear expression in a solver response.

-   int64
    [SolutionIntegerMin](#_namespaceoperations__research_1_1sat_1a671200a31003492dbef21f2b4ee3dcbd)
    (const CpSolverResponse &r,
    [IntVar](#_classoperations__research_1_1sat_1_1IntVar) x)

    Returns the min of an integer variable in a solution.

-   int64
    [SolutionIntegerMax](#_namespaceoperations__research_1_1sat_1a8ec893fa736de5b95135ecb9314ee6d8)
    (const CpSolverResponse &r,
    [IntVar](#_classoperations__research_1_1sat_1_1IntVar) x)

    Returns the max of an integer variable in a solution.

-   bool
    [SolutionBooleanValue](#_namespaceoperations__research_1_1sat_1afa415e372a9d64eede869ed98666c29c)
    (const CpSolverResponse &r,
    [BoolVar](#_classoperations__research_1_1sat_1_1BoolVar) x)

-   std::string
    [CpModelStats](#_namespaceoperations__research_1_1sat_1a287579e5f181fc7c89feccf1128faffb)
    (const CpModelProto &model)

    Returns a std::string with some statistics on the given
    CpModelProto.

-   std::string
    [CpSolverResponseStats](#_namespaceoperations__research_1_1sat_1ac2d87e8109f9c60f7af84a60106abd57)
    (const CpSolverResponse &response)

    Returns a std::string with some statistics on the solver response.

-   CpSolverResponse
    [SolveCpModel](#_namespaceoperations__research_1_1sat_1a9d67b9c66f1cb9c1dcc3415cd5af11bf)
    (const CpModelProto &model\_proto,
    [Model](#_classoperations__research_1_1sat_1_1Model) \*model)

-   CpSolverResponse
    [Solve](#_namespaceoperations__research_1_1sat_1a09d851f944ab4f305c3d9f8df99b7bf8)
    (const CpModelProto &model\_proto)

    Solves the given cp\_model and returns an instance of
    CpSolverResponse.

-   CpSolverResponse
    [SolveWithParameters](#_namespaceoperations__research_1_1sat_1aa3062797aa0396abf37dbcc99a746f12)
    (const CpModelProto &model\_proto, const SatParameters &params)

-   CpSolverResponse
    [SolveWithParameters](#_namespaceoperations__research_1_1sat_1af52c27ecb43d6486c1a70e022b4aad39)
    (const CpModelProto &model\_proto, const std::string &params)

-   void
    [SetSynchronizationFunction](#_namespaceoperations__research_1_1sat_1ad04337634227eac006d3e33a7028f82f)
    (std::function\< CpSolverResponse()\> f,
    [Model](#_classoperations__research_1_1sat_1_1Model) \*model)

<!-- -->

-   std::function\<
    void([Model](#_classoperations__research_1_1sat_1_1Model) \*)\>
    [NewFeasibleSolutionObserver](#_namespaceoperations__research_1_1sat_1aacf15d440f0db4cd0a63c8aebe85db6d)
    (const std::function\< void(const CpSolverResponse &response)\>
    &observer)

-   std::function\<
    SatParameters([Model](#_classoperations__research_1_1sat_1_1Model)
    \*)\>
    [NewSatParameters](#_namespaceoperations__research_1_1sat_1a10700832ca6bc420f2931eb707957b0b)
    (const std::string &params)

Function Documentation
----------------------

### CpModelStats()

CpModelStats

operations\_research::sat

operations\_research::sat

CpModelStats

`std::string operations_research::sat::CpModelStats (const CpModelProto & model)`

Returns a std::string with some statistics on the given CpModelProto.

### CpSolverResponseStats()

CpSolverResponseStats

operations\_research::sat

operations\_research::sat

CpSolverResponseStats

`std::string operations_research::sat::CpSolverResponseStats (const CpSolverResponse & response)`

Returns a std::string with some statistics on the solver response.

### Not()

Not

operations\_research::sat

operations\_research::sat

Not

`BoolVar operations_research::sat::Not (BoolVar x)`

A convenient wrapper so we can write Not(x) instead of
[x.Not()](#_namespaceoperations__research_1_1sat_1a5e3de118c1f8dd5a7ec21704e05684b9)
which is sometimes clearer.

### operator\<\<()`[1/3]`

operator\<\<

operations\_research::sat

operations\_research::sat

operator\<\<

`std::ostream& operations_research::sat::operator<< (std::ostream & os, const BoolVar & var)`

### operator\<\<()`[2/3]`

operator\<\<

operations\_research::sat

operations\_research::sat

operator\<\<

`std::ostream& operations_research::sat::operator<< (std::ostream & os, const IntVar & var)`

### operator\<\<()`[3/3]`

operator\<\<

operations\_research::sat

operations\_research::sat

operator\<\<

`std::ostream& operations_research::sat::operator<< (std::ostream & os, const IntervalVar & var)`

### SetSynchronizationFunction()

SetSynchronizationFunction

operations\_research::sat

operations\_research::sat

SetSynchronizationFunction

`void operations_research::sat::SetSynchronizationFunction (std::function< CpSolverResponse()> f, Model * model)`

If set, the underlying solver will call this function \"regularly\" in a
deterministic way. It will then wait until this function returns with
the current \"best\" information about the current problem.

This is meant to be used in a multi-threaded environment with many
parallel solving process. If the returned current \"best\" response only
uses informations derived at a lower deterministic time (possibly with
offset) than the deterministic time of the current thread, then the
whole process can be made deterministic.

### SolutionBooleanValue()

SolutionBooleanValue

operations\_research::sat

operations\_research::sat

SolutionBooleanValue

`bool operations_research::sat::SolutionBooleanValue (const CpSolverResponse & r, BoolVar x)`

Returns the value of a Boolean literal (a Boolean variable or its
negation) in a solver response.

### SolutionIntegerMax()

SolutionIntegerMax

operations\_research::sat

operations\_research::sat

SolutionIntegerMax

`int64 operations_research::sat::SolutionIntegerMax (const CpSolverResponse & r, IntVar x)`

Returns the max of an integer variable in a solution.

### SolutionIntegerMin()

SolutionIntegerMin

operations\_research::sat

operations\_research::sat

SolutionIntegerMin

`int64 operations_research::sat::SolutionIntegerMin (const CpSolverResponse & r, IntVar x)`

Returns the min of an integer variable in a solution.

### SolutionIntegerValue()

SolutionIntegerValue

operations\_research::sat

operations\_research::sat

SolutionIntegerValue

`int64 operations_research::sat::SolutionIntegerValue (const CpSolverResponse & r, const LinearExpr & expr)`

Evaluates the value of an linear expression in a solver response.

### Solve()

Solve

operations\_research::sat

operations\_research::sat

Solve

`CpSolverResponse operations_research::sat::Solve (const CpModelProto & model_proto)`

Solves the given cp\_model and returns an instance of CpSolverResponse.

### SolveCpModel()

SolveCpModel

operations\_research::sat

operations\_research::sat

SolveCpModel

`CpSolverResponse operations_research::sat::SolveCpModel (const CpModelProto & model_proto, Model * model)`

Solves the given CpModelProto.

> **Note**
>
> the API takes a Model\* that will be filled with the in-memory
> representation of the given CpModelProto. It is done this way so that
> it is easy to set custom parameters or interrupt the solver will calls
> like:
>
> -   model-\>Add(NewSatParameters(parameters\_as\_string\_or\_proto));
>
> -   model-\>GetOrCreate\<TimeLimit\>()-\>RegisterExternalBooleanAsLimit(&stop);
>
> -   model-\>GetOrCreate\<SigintHandler\>()-\>Register(\[&stop\]() {
>     stop = true; });
>
### SolveWithParameters()`[1/2]`

SolveWithParameters

operations\_research::sat

operations\_research::sat

SolveWithParameters

`CpSolverResponse operations_research::sat::SolveWithParameters (const CpModelProto & model_proto, const SatParameters & params)`

Solves the given cp\_model with the give sat parameters, and returns an
instance of CpSolverResponse.

### SolveWithParameters()`[2/2]`

SolveWithParameters

operations\_research::sat

operations\_research::sat

SolveWithParameters

`CpSolverResponse operations_research::sat::SolveWithParameters (const CpModelProto & model_proto, const std::string & params)`

Solves the given cp\_model with the given sat parameters as std::string
in JSon format, and returns an instance of CpSolverResponse.

Variable Documentation
----------------------

### NewFeasibleSolutionObserver

NewFeasibleSolutionObserver

operations\_research::sat

operations\_research::sat

NewFeasibleSolutionObserver

`std::function<void(Model*)> operations_research::sat::NewFeasibleSolutionObserver(const std::function< void(const CpSolverResponse &response)> &observer)`

Allows to register a solution \"observer\" with the model with
model.Add(NewFeasibleSolutionObserver(\[\](response){\...})); The given
function will be called on each \"improving\" feasible solution found
during the search. For a non-optimization problem, if the option to find
all solution was set, then this will be called on each new solution.

### NewSatParameters

NewSatParameters

operations\_research::sat

operations\_research::sat

NewSatParameters

`std::function< SatParameters(Model *)> operations_research::sat::NewSatParameters`

Allows to change the default parameters with
model-\>Add(NewSatParameters(parameters\_as\_string\_or\_proto)) before
calling
[SolveCpModel()](#_namespaceoperations__research_1_1sat_1a9d67b9c66f1cb9c1dcc3415cd5af11bf).

ortools/util/sorted\_interval\_list.h File Reference {#_sorted__interval__list_8h}
====================================================

ortools/util/sorted\_interval\_list.h

    #include <iostream>
    #include <set>
    #include <string>
    #include <vector>
    #include "absl/container/inlined_vector.h"
    #include "absl/types/span.h"
    #include "ortools/base/integral_types.h"

Include dependency graph for sorted\_interval\_list.h:

This graph shows which files directly or indirectly include this file:

-   struct
    [operations\_research::ClosedInterval](#_structoperations__research_1_1ClosedInterval)

    Represents a closed interval \[start, end\]. We must have start \<=
    end.

-   class
    [operations\_research::Domain](#_classoperations__research_1_1Domain)

-   class
    [operations\_research::SortedDisjointIntervalList](#_classoperations__research_1_1SortedDisjointIntervalList)

-   struct
    [operations\_research::SortedDisjointIntervalList::IntervalComparator](#_structoperations__research_1_1SortedDisjointIntervalList_1_1IntervalComparator)

<!-- -->

-   [operations\_research](#_namespaceoperations__research)

<!-- -->

-   std::ostream &
    [operations\_research::operator\<\<](#_namespaceoperations__research_1a5c341d9214d5d46014089435ba0e26d3)
    (std::ostream &out, const ClosedInterval &interval)

-   std::ostream &
    [operations\_research::operator\<\<](#_namespaceoperations__research_1aaa301d39d2a9271daf8c65e779635335)
    (std::ostream &out, const std::vector\< ClosedInterval \>
    &intervals)

-   bool
    [operations\_research::IntervalsAreSortedAndNonAdjacent](#_namespaceoperations__research_1ab8c23924c4b61ed5c531424a6f18bde1)
    (absl::Span\< const ClosedInterval \> intervals)

-   std::ostream &
    [operations\_research::operator\<\<](#_namespaceoperations__research_1abebf3070a940da6bf678953a66584e76)
    (std::ostream &out, const Domain &domain)

operations\_research::ClosedInterval Struct Reference {#_structoperations__research_1_1ClosedInterval}
=====================================================

operations\_research::ClosedInterval

Represents a closed interval \[start, end\]. We must have start \<= end.

`#include <sorted_interval_list.h>`

-   [ClosedInterval](#_structoperations__research_1_1ClosedInterval_1ac88ed3db6ef14b84e38b5d89d39aaa04)
    ()

-   [ClosedInterval](#_structoperations__research_1_1ClosedInterval_1ae79c6820d4e756c8805ef8f3dac20655)
    (int64 s, int64 e)

-   std::string
    [DebugString](#_structoperations__research_1_1ClosedInterval_1a45209c3e31f989a7d5b3c7f8a15f6164)
    () const

-   bool
    [operator==](#_structoperations__research_1_1ClosedInterval_1a5e40a5426c5178a044f8147e05a57e67)
    (const
    [ClosedInterval](#_structoperations__research_1_1ClosedInterval)
    &other) const

-   bool
    [operator\<](#_structoperations__research_1_1ClosedInterval_1aec81d0fbae7b13a421f4aebc0285df2d)
    (const
    [ClosedInterval](#_structoperations__research_1_1ClosedInterval)
    &other) const

<!-- -->

-   int64
    [start](#_structoperations__research_1_1ClosedInterval_1add276e813566f507dc7b02a1971ea60b)
    = 0

-   int64
    [end](#_structoperations__research_1_1ClosedInterval_1a650dc1bae78be4048e0f8e8377208925)
    = 0

Detailed Description
--------------------

Represents a closed interval \[start, end\]. We must have start \<= end.

Constructor & Destructor Documentation
--------------------------------------

### ClosedInterval()`[1/2]`

ClosedInterval

operations\_research::ClosedInterval

operations\_research::ClosedInterval

ClosedInterval

`operations_research::ClosedInterval::ClosedInterval ( )[inline]`

### ClosedInterval()`[2/2]`

ClosedInterval

operations\_research::ClosedInterval

operations\_research::ClosedInterval

ClosedInterval

`operations_research::ClosedInterval::ClosedInterval (int64 s, int64 e)[inline]`

Member Function Documentation
-----------------------------

### DebugString()

DebugString

operations\_research::ClosedInterval

operations\_research::ClosedInterval

DebugString

`std::string operations_research::ClosedInterval::DebugString ( ) const`

### operator\<()

operator\<

operations\_research::ClosedInterval

operations\_research::ClosedInterval

operator\<

`bool operations_research::ClosedInterval::operator< (const ClosedInterval & other) const[inline]`

Because we mainly manipulate vector of disjoint intervals, we only need
to sort by the start. We do not care about the order in which interval
with the same start appear since they will always be merged into one
interval.

### operator==()

operator==

operations\_research::ClosedInterval

operations\_research::ClosedInterval

operator==

`bool operations_research::ClosedInterval::operator== (const ClosedInterval & other) const[inline]`

Member Data Documentation
-------------------------

### end

end

operations\_research::ClosedInterval

operations\_research::ClosedInterval

end

`int64 operations_research::ClosedInterval::end = 0`

### start

start

operations\_research::ClosedInterval

operations\_research::ClosedInterval

start

`int64 operations_research::ClosedInterval::start = 0`

The documentation for this struct was generated from the following file:

ortools/util/

sorted\_interval\_list.h

operations\_research::SortedDisjointIntervalList::IntervalComparator Struct Reference {#_structoperations__research_1_1SortedDisjointIntervalList_1_1IntervalComparator}
=====================================================================================

operations\_research::SortedDisjointIntervalList::IntervalComparator

`#include <sorted_interval_list.h>`

-   bool
    [operator()](#_structoperations__research_1_1SortedDisjointIntervalList_1_1IntervalComparator_1ab348be2102e6d84cf3d994dac9afd657)
    (const
    [ClosedInterval](#_structoperations__research_1_1ClosedInterval) &a,
    const
    [ClosedInterval](#_structoperations__research_1_1ClosedInterval) &b)
    const

Member Function Documentation
-----------------------------

### operator()()

operator()

operations\_research::SortedDisjointIntervalList::IntervalComparator

operations\_research::SortedDisjointIntervalList::IntervalComparator

operator()

`bool operations_research::SortedDisjointIntervalList::IntervalComparator::operator() (const ClosedInterval & a, const ClosedInterval & b) const[inline]`

The documentation for this struct was generated from the following file:

ortools/util/

sorted\_interval\_list.h

 {#_todo}

Member [operations\_research::Domain::intervals](#_classoperations__research_1_1Domain_1a153837b45a0e9ddf5620679f243baa96) () const

:   (user): remove, this makes a copy and is of a different type that
    our internal InlinedVector() anyway.

Member [operations\_research::sat::CpModelBuilder::Build](#_classoperations__research_1_1sat_1_1CpModelBuilder_1a8e1b64644f124be491431bbae9d5d843) () const

:   (user) : add MapDomain?

Member [operations\_research::sat::Model::GetOrCreate](#_classoperations__research_1_1sat_1_1Model_1af4eb422b7cfd963c58d65b18b4e47717) ()

:   (user): directly store std::unique\_ptr\<\> in singletons\_?

Class [operations\_research::SortedDisjointIntervalList](#_classoperations__research_1_1SortedDisjointIntervalList)

:   (user): Templatize the class on the type of the bounds.

Member [operations\_research::SortedDisjointIntervalList::SortedDisjointIntervalList](#_classoperations__research_1_1SortedDisjointIntervalList_1a85b8188b348ebff82e5ff38ef49da201) (const std::vector\< int64 \> &starts, const std::vector\< int64 \> &ends)

:   (user): Explain why we favored this API to the more natural input
    std::vector\<ClosedInterval\> or std::vector\<std::pair\<int,
    int\>\>.
