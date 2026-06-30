# Multikey Radix Sort in OR-Tools

This document explains the design, variants, and usage of the high-performance
Radix Sort implementations provided in
[`multikey_radix_sort.h`](./multikey_radix_sort.h).

---

## 1. Summary Decision Tree

```
Do you know the exact min/max bounds of your keys in advance?
 ├── YES: Use Range*
 └── NO: Are you sorting by multiple keys (e.g., tuples, compound structs)?
      ├── YES: Use Multikey*
      └── NO: Use Auto*

For your chosen variant (Range, Multikey, or Auto):
Is key extraction expensive OR does your data require >= 3 passes?
  ├── YES: Use *HistogramRadixSort (e.g., AutoHistogramRadixSort)
  └── NO: Use standard *RadixSort (e.g., AutoRadixSort)
```

---

## 2. The API Naming Matrix

The library provides a flexible family of sorting algorithms structured around
three orthogonal design dimensions. The function names follow the pattern:

`API Name = [Auto | Range | Multikey] [ (Standard) | Histogram ] RadixSort`

### Dimension 1: Key Range & Access Strategy (`Auto` vs. `Range` vs. `Multikey`)

This dimension dictates how the minimum and maximum key bounds are determined,
and whether you are sorting by single or multiple keys.

*   **`Auto*RadixSort(container, [get_key])`**
    *   **Behavior:** Performs an initial O(N) scanning pass over the
        container (`ComputeMinMax`) to find the exact `key_min` and `key_max`.
    *   **When/Why to use:** Use this when the range of keys is unknown,
        highly variable, or dynamic. It ensures the radix passes are perfectly
        tailored to the actual data span, avoiding redundant passes over
        leading zero bits.

*   **`Range*RadixSort(key_min, key_max, container, [get_key])`**
    *   **Behavior:** Accepts explicit `key_min` and `key_max` bounds directly
        from the caller, skipping the initial O(N) min/max scanning pass.
    *   **When/Why to use:** Use this when the domain bounds are known
        statically or guaranteed by prior logic (e.g., sorting array indices
        known to be in `[0, 100000]`). If you know the bounds in advance, using
        a `Range` variant is almost always a performance win because eliminating
        the initial scanning pass saves significant memory bandwidth and CPU
        cycles.

*   **`Multikey*RadixSort(container, get_key1, get_key2, ...)`**
    *   **Behavior:** Sorts elements by multiple keys in lexicographical order.
        For each key, the minimum and maximum bounds are computed dynamically
        via an initial O(N) scan, exactly like in `Auto`. Because Least
        Significant Digit (LSD) radix sort is stable, the key accessors must be
        provided in **reverse order of importance** (least significant key
        first, primary key last).
    *   **When/Why to use:** Use this for compound sorting (e.g., sorting
        tuples or structs primarily by field `A`, secondarily by field `B`).
        Note that `Multikey*RadixSort` behavior can be perfectly emulated
        (modulo the memory savings) by calling `AutoRadixSort` or
        `RangeRadixSort` several times on the same container, starting from the
        least important key. However, `Multikey*RadixSort` is highly optimized
        to reuse a single allocated temporary buffer across all passes for all
        keys, drastically reducing memory allocation overhead compared to
        standalone sequential calls.

---

### Dimension 2: Histogram Strategy (`Standard` vs. `Histogram`)

This dimension dictates how counting passes are performed across the multiple
radix digits (passes) required to sort the data.

*   **Standard Radix Sort (`AutoRadixSort`, `RangeRadixSort`, `MultikeyRadixSort`)**
    *   **Behavior:** For each radix pass (typically 11 to 13 bits per pass),
        it performs a counting pass over the data, computes the prefix sums,
        and then performs a distribution pass. If sorting requires K passes,
        it makes K counting passes and K distribution passes over the data.
    *   **When/Why to use:** Excellent when L1/L2 cache locality during
        counting is critical, or when the data width requires only 1 or 2
        passes.

*   **Histogram Radix Sort (`AutoHistogramRadixSort`, `RangeHistogramRadixSort`,
    `MultikeyHistogramRadixSort`)**
    *   **Behavior:** Performs a single, consolidated initial counting pass that
        computes the histograms for **all K passes simultaneously**. It then
        executes the K distribution passes.
    *   **When/Why to use (Recommended for most multi-pass workloads):** Use
        this when key extraction is computationally expensive (e.g., complex
        pointer chasing, dereferencing, or floating-point bijections). By
        evaluating `get_key(item)` only once during the counting phase for
        all passes, it significantly reduces CPU overhead and memory reads.

---

## 3. Supported Data Types

The library supports sorting containers whose extracted keys are:

1.  **Integers:** Signed and unsigned integers up to 64 bits (`int32_t`,
    `uint64_t`, etc.). Two's complement arithmetic is fully supported.
2.  **Enumerations:** C++ `enum` and `enum class` types.
3.  **Floating-Point Numbers (`float`, `double`):**
    *   Floating-point keys are correctly and stably sorted using an
        order-preserving bitwise bijection (`FloatToSortableUint` /
        `SortableUintToFloat`).
    *   IEEE 754 floating-point representation is transformed such that negative
        numbers (including `-inf`), `-0.0`, `+0.0`, positive numbers, and NaNs
        sort in the correct mathematical continuum.

---

## 4. Sorting Direction (`RadixSortDirection`)

All API functions accept an optional template parameter,
`RadixSortDirection Direction`, which defaults to
`RadixSortDirection::kIncreasing`.

```cpp
enum class RadixSortDirection {
  kIncreasing,
  kDecreasing,
};
```

### Example Usage
```cpp
// Sort in ascending order (default)
AutoHistogramRadixSort(values, [](const Item& item) { return item.score; });

// Sort in descending order
AutoHistogramRadixSort<RadixSortDirection::kDecreasing>(
    values, [](const Item& item) { return item.score; });
```

### How Direction Works Under the Hood
Instead of duplicating the counting and distribution loops with reverse indexing
logic, `multikey_radix_sort.h` elegantly transforms the extracted radix key at
the lowest level (`GetRadixKey`).

*   **Increasing (`kIncreasing`):** The integer key is shifted by the minimum
    value: `key - key_min` (or
    `FloatToSortableUint(key) - FloatToSortableUint(key_min)` for floating-point
    types).
*   **Decreasing (`kDecreasing`):** The integer key is inverted relative to the
    maximum value: `key_max - key` (or
    `FloatToSortableUint(key_max) - FloatToSortableUint(key)` for floating-point
    types). This maps the largest original keys to the smallest radix buckets,
    achieving a perfectly stable descending sort with zero branching overhead in
    the core loops.

---

## 5. Permutation-Based Indirect Sorting

When sorting a container of large, move-expensive, or move-only objects,
or when the keys are stored in an independent data structure that is mapped
by index, you can compute a permutation of indices instead of sorting the
original container directly.

The library provides two families of index-computing permutation APIs:
the `Compute` variants that return a new `RadixSortPermutation` by value,
and the `Update` variants that modify an existing `RadixSortPermutation`
in-place via a non-const reference parameter.

*   `ComputeAutoRadixSortPermutation` /
    `ComputeAutoHistogramRadixSortPermutation`
    (creates a new permutation; variadic, supports multikey)
*   `UpdateAutoRadixSortPermutation` /
    `UpdateAutoHistogramRadixSortPermutation`
    (updates permutation in-place; variadic, supports multikey)
*   `ComputeRangeRadixSortPermutation` /
    `ComputeRangeHistogramRadixSortPermutation`
    (creates a new permutation; single-key only)
*   `UpdateRangeRadixSortPermutation` /
    `UpdateRangeHistogramRadixSortPermutation`
    (updates permutation in-place; single-key only)

> [!NOTE]
> The `Range`-based APIs are restricted to a single key accessor. Because
> the caller must supply explicit `key_min` and `key_max` bounds, specifying
> bounds for multiple keys directly in the signature would result in a
> complex and cluttered API signature. Multi-key lexicographical range
> sorting is better served by the `Auto` variants or sequential single-key
> sorting passes.

The `Compute` functions return a `RadixSortPermutation` object holding a
`std::vector<std::size_t>`. You can cascade sorting passes by passing an
existing `RadixSortPermutation` to the `Update` functions, which is then
updated in-place by subsequent key sweeps:

```cpp
// 1. Initial pass sorting by secondary key
RadixSortPermutation perm = ComputeAutoRadixSortPermutation(
    container, [&](const std::size_t i) { return secondary_keys[i]; });

// 2. Cascade sort by primary key
UpdateAutoRadixSortPermutation(
    perm, container, [&](const std::size_t i) { return primary_keys[i]; });

// 3. Apply the final sorted permutation
perm.ApplyTo(container);
```

