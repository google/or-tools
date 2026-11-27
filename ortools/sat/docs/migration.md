[home](README.md) | [boolean logic](boolean_logic.md) | [integer arithmetic](integer_arithmetic.md) | [channeling constraints](channeling.md) | [scheduling](scheduling.md) | [Using the CP-SAT solver](solver.md) | [Model manipulation](model.md) | [Troubleshooting](troubleshooting.md) | [Migration](migration.md) | [Python API](https://or-tools.github.io/docs/pdoc/ortools/sat/python/cp_model.html)
----------------- | --------------------------------- | ------------------------------------------- | --------------------------------------- | --------------------------- | ------------------------------------ | ------------------------------ | ------------------------------------- | ------------------------- | -----------------------------------------------------------------------------------
# Migrating the CP-SAT Solver from v9.15 to v10.0-rc.1

This guide summarizes the major CP-SAT changes since **v9.15** and explains how
to migrate CP-SAT code from OR-Tools **v9.15** to **v10.0-rc.1** across **C++**,
**.NET (C#)**, **Java**, and **Python**.

Unlike the Routing library extraction in v10.0-rc.1, the core CP-SAT modeling
APIs (`CpModel` / `CpModelBuilder` and `CpSolver`) remain largely
source-compatible. Migration effort centers on **platform/toolchain upgrades**,
**Java's move to SWIG-generated CP-SAT protobuf wrappers**, **Python's newly
enabled `DeprecationWarning` for legacy CamelCase methods**, **C++
`CpModelBuilder` copy/move restrictions and Abseil logging**, and
**protobuf/parameter cleanups**.

--------------------------------------------------------------------------------

## Summary of Major Changes Since v9.15

### Solver and Algorithmic Improvements

*   **Dedicated scheduling local search (`ls_scheduling`)**: Added a specialized
    subsolver implementing Nowicki & Smutnicki's tabu search for job-shop and
    machine scheduling problems.
*   **Expanded probing, shaving, and branching portfolio**: Added
    `ContinuousProber` and new shaving/probing/branching subsolvers (`shaving*`,
    `probing_no_lp`, `alt_fixed*`, `max_hs_*`, `core_or_*`), along with
    `ROUND_ROBIN_SHAVING_SEARCH` and `ALT_FIXED_SEARCH` branching modes.
*   **Parallel direct solve & solution crushing**: Added optional unpresolved
    direct solving (`cp_model_direct_solve`) running concurrently with presolve,
    and formalized solution crushing via `solution_crush.proto`.
*   **SCIP-free `MaxHS`**: Reimplemented `optimize_with_max_hs` using an inner
    CP-SAT solver, removing its dependency on SCIP.
*   **Floating-point objective and MIP scaling overhaul**: Made floating-point
    objective values and live bound updates exact during search; added residual
    continuous component analysis with TUM/2-modular detection, precision-driven
    power-of-two scaling (`mip_scale_for_precision`), dual-reasoning bound
    restriction (`mip_presolve_restrict_bounds`), tight discretization
    (`mip_proper_relaxation`), and optional post-solve verification
    (`mip_verify_solution`).
*   **Stronger LP relaxations and MIP cuts**: Added Boolean equivalence
    substitution in LP (`use_boolean_equivalences_in_lp`), GUB lifting, Boolean
    bumping, Letchford-Souli knapsack cover cuts, tight upper implied bounds in
    cuts (`use_tight_upper_implied_bounds_in_cuts`), generalized `lin_max` cuts,
    and upfront GCD simplification, density capping, and sparsification in
    `LinearConstraintManager`.
*   **New integer conflict resolution enabled by default**:
    `use_new_integer_conflict_resolution` now defaults to `true` (activating
    when a model has at least 10 non-binary integer variables).
*   **Presolve and constraint upgrades**: Added general `LinearExpressionProto`
    support to `InverseConstraintProto` (`f_expr_direct` / `f_expr_inverse`),
    new linear presolve rules (2-term tilting, forced big-M decoupling, knapsack
    redundancy detection, complementary `lin_max`, linking-variable elimination,
    and complex-domain Boolean elimination), and new 2D packing/scheduling
    presolve rules (`PresolveNoOverlap2DFramed`, `ExpandEncoded2DBinPacking`,
    splitting `cumulative` with precedences).
*   **LNS, Violation LS, and Shared Tree rework**: Reworked `ModelCopy` /
    `DenseModelCopy` with adaptive LNS presolve deterministic limits,
    rate-limited Violation LS rebuilds, optional skipping of redundant
    scheduling constraints in LNS and Feasibility Jump, and a reworked Shared
    Tree engine (`SharedTreeEncoder`, per-node lower-bound sharing, LRAT reason
    export, and sync throttling).
*   **SAT inprocessing and LRAT proof verification**: Extracted
    `GateCongruenceClosure`, added experimental circuit sweeping and
    multi-ordering vivification (`num_ordering_in_vivification`), and expanded
    LRAT proof tracking/checking (`cp_model_pure_sat_presolve`, merged
    multi-worker LRAT verification, `lrat_checker_main`), replacing DRAT.

### Breaking and API Highlights

*   **Platform & dependency bumps**: C++20 required, JDK 21+ required, Python
    3.10–3.15 (Python 3.9 dropped), `.NET 8.0` LTS (official NuGet packages drop
    `net462`/`net472`), Protobuf v35.1 (`protobuf>=7.35.0,<8.0` in Python),
    Abseil `20260817.0`, and Boost removed.
*   **Java CP-SAT protos migrated to SWIG wrappers (#5066)**: `CpModelProto`,
    `CpSolverResponse`, and related `cp_model.proto` types are now
    SWIG-generated C++ wrappers rather than `protobuf-java` `GeneratedMessage`
    classes (`SatParameters` remains standard `protobuf-java`).
*   **Python `DeprecationWarning` enabled by default**: Calling legacy pre-PEP 8
    CamelCase methods (`NewIntVar`, `AddBoolOr`, `Solve`, `Value`, etc.) now
    emits `DeprecationWarning` by default (`cp_model.enable_warnings` was
    removed). `add_reservoir_constraint` also validates bounds and non-empty
    `times` upfront, and `Constraint.only_enforce_if` enforces stricter Boolean
    literal typing.
*   **C++ API & header changes**: Deleted copy/move assignment on
    `CpModelBuilder` (use `Clone()`), added fluent specialized constraint
    builders (`BoolOrConstraint`, `AtMostOneConstraint`, `ExactlyOneConstraint`,
    `BoolAndConstraint`, `BoolXorConstraint`, `NoOverlapConstraint`), added
    `BoolVar::IsFixedTo{True,False}()` and experimental `NewSubsolver`, and
    removed legacy `ortools/base/logging.h` in favor of Abseil logging.
*   **Moved and removed protos & parameters**: Moved
    `boolean_problem.{h,cc,proto}` from `ortools/sat/` to `ortools/bop/`,
    deleted `ortools/sat/cp_model_service.proto`, removed DRAT proof
    parameters/writer (`check_drat_proof`, `output_drat_proof`,
    `max_drat_time_in_seconds`, `drat_writer.{h,cc}`), and replaced
    `inprocessing_minimization_use_all_orderings` with
    `num_ordering_in_vivification`.

### Notable Bug Fixes

*   Fixed v9.15 Python regressions in `CpSolver.status_name()` (#4985) and the
    `CpSolver.Solve(..., solution_callback=...)` keyword argument (#4988), as
    well as `Constraint.only_enforce_if()` return values and type hints (#5568).
*   Fixed shaving solver deadlocks (#5532, #5541), `-1` interval handling
    (#5327), and log spam (#5163).
*   Fixed non-determinism in interleaved LNS (#5423), silent loss of enforcement
    literals in presolve (#5294), `int_div` multi-term divisor validation
    (#5472), exponential domain blowup in `DomainSuperSetOf` (#5550), 64-bit
    integer overflows in cut generators and `SumOfAllDiffLowerBounder`,
    non-finite floating-point objective scaling, `bool_xor` model copy, and
    scheduling/2D explanation bugs.
*   Allowed disabling `no_overlap` to `all_diff` presolve conversion via
    `infer_all_diffs` (#5337) and excluded `lrat_checker_main.cc` from
    `libortools` (#5086).

--------------------------------------------------------------------------------

## Contents

*   [Summary of Major Changes Since v9.15](#summary-of-major-changes-since-v915)
*   [Platform and Toolchain Requirements](#platform-and-toolchain-requirements)
*   [Quick Comparison Table](#quick-comparison-table)
*   [Protobuf Schema and Search Parameter Changes](#protobuf-schema-and-search-parameter-changes)
*   [C++ Migration Guide](#c-migration-guide)
*   [.NET (C#) Migration Guide](#net-c-migration-guide)
*   [Java Migration Guide](#java-migration-guide)
*   [Python Migration Guide](#python-migration-guide)
*   [References](#references)

--------------------------------------------------------------------------------

## Platform and Toolchain Requirements

| Language   | v9.15                         | v10.0-rc.1                     |
| :--------- | :---------------------------- | :----------------------------- |
| **C++**    | C++17 (Linux/macOS), CMake >= | **C++20 required** (GCC >= 10, |
:            : 3.24                          : MSVC >= 2022); CMake >= 3.25;  :
:            :                               : Bazel 8.7.0; Boost removed     :
| **.NET**   | `.NET 8.0`, `.NET Framework`  | **`.NET 8.0` LTS** (official   |
:            : (`net462`/`net472`)           : NuGet packages drop            :
:            :                               : `net462`/`net472`);            :
:            :                               : `Google.Protobuf` 3.35.1       :
| **Java**   | JDK 8 / 11 / 17,              | **JDK 21+ required**;          |
:            : `protobuf-java` for           : `protobuf-java` 4.35.1;        :
:            : `CpModelProto`                : SWIG-generated wrappers for    :
:            :                               : `cp_model.proto`               :
| **Python** | Python 3.9–3.14, `pybind11`   | **Python 3.10–3.15**,          |
:            : v2.13, `protobuf` 6.33.x      : **`pybind11` v3.1**,           :
:            :                               : `protobuf>=7.35.0,<8.0`        :

--------------------------------------------------------------------------------

## Quick Comparison Table

| Concept                      | Language | v9.15                                         | v10.0-rc.1                             |
| :--------------------------- | :------- | :-------------------------------------------- | :------------------------------------- |
| **Logging header & init**    | C++      | `#include "ortools/base/logging.h"`           | `#include "absl/log/log.h"`,           |
:                              :          :                                               : `"absl/log/check.h"`,                  :
:                              :          :                                               : `"absl/log/globals.h"`,                :
:                              :          :                                               : `"ortools/base/log_severity.h"`        :
| **Copying `CpModelBuilder`** | C++      | Implicit copy/move (unsafe dangling pointers) | Copy/move deleted/private; use         |
:                              :          :                                               : `CpModelBuilder copy = model.Clone();` :
| **Constraint chaining**      | C++      | `AddArc`/`AddTuple`/`AddDemand` returned      | Return `*this` for chaining;           |
:                              :          : `void`; `AddBoolOr`/`AddNoOverlap` returned   : `BoolOrConstraint`,                    :
:                              :          : `Constraint`                                  : `AtMostOneConstraint`,                 :
:                              :          :                                               : `ExactlyOneConstraint`,                :
:                              :          :                                               : `BoolAndConstraint`,                   :
:                              :          :                                               : `BoolXorConstraint`,                   :
:                              :          :                                               : `NoOverlapConstraint`                  :
| **`boolean_problem.proto`**  | All      | `ortools/sat/boolean_problem.proto`           | `ortools/bop/boolean_problem.proto`    |
:                              :          : (`operations_research.sat`)                   : (`operations_research.bop`)            :
| **`cp_model_service.proto`** | All      | `ortools/sat/cp_model_service.proto`          | Removed (define your own RPC wrapper   |
:                              :          : (`operations_research.sat.v1`)                : if needed)                             :
| **Proof checking params**    | All      | `check_drat_proof`, `output_drat_proof`,      | Removed; use LRAT (`check_lrat_proof`, |
:                              :          : `max_drat_time_in_seconds`                    : `output_lrat_proof`,                   :
:                              :          :                                               : `check_merged_lrat_proof`,             :
:                              :          :                                               : `cp_model_pure_sat_presolve`)          :
| **Vivification orderings**   | All      | `inprocessing_minimization_use_all_orderings` | Removed; use                           |
:                              :          : (bool)                                        : `num_ordering_in_vivification` (int32, :
:                              :          :                                               : default `1`)                           :
| **`CpModelProto` /           | Java     | `protobuf-java` `GeneratedMessage`            | SWIG C++ proto wrappers                |
: `CpSolverResponse`**         :          : (`//ortools/sat\:cp_model_java_proto`)        : (`//ortools/sat/java\:cp_model_proto`; :
:                              :          :                                               : requires                               :
:                              :          :                                               : `Loader.loadNativeLibraries()` first)  :
| **Legacy CamelCase methods** | Python   | Silent unless `cp_model.enable_warnings =     | `DeprecationWarning` enabled by        |
:                              :          : True`                                         : default (`enable_warnings` removed);   :
:                              :          :                                               : migrate to PEP 8 `snake_case`          :
| **`Domain` type stubs**      | Python   | No `.pyi` stub                                | `sorted_interval_list.pyi` added + new |
:                              :          :                                               : `Domain` helpers (`empty`, `values`,   :
:                              :          :                                               : `value_at_or_before`,                  :
:                              :          :                                               : `value_at_or_after`, `__eq__`, etc.)   :

--------------------------------------------------------------------------------

## Protobuf Schema and Search Parameter Changes

These changes apply across all four languages as well as serialized or textproto
`CpModelProto`, `CpSolverResponse`, and `SatParameters` configurations.

### 1. Moved and Removed Protobuf Files

| v9.15                                | v10.0-rc.1                           |
| :----------------------------------- | :----------------------------------- |
| `ortools/sat/boolean_problem.proto`  | `ortools/bop/boolean_problem.proto`  |
: (`package operations_research.sat;`) : (`package operations_research.bop;`) :
| `option java_package =               | `option java_package =               |
: "com.google.ortools.sat";` (for      : "com.google.ortools.bop";`           :
: `boolean_problem.proto`)             :                                      :
| `option csharp_namespace =           | `option csharp_namespace =           |
: "Google.OrTools.Sat";` (for          : "Google.OrTools.Bop";`               :
: `boolean_problem.proto`)             :                                      :
| `ortools/sat/cp_model_service.proto` | **Removed** (define an               |
: (`CpSolver` gRPC service &           : application-specific request proto   :
: `CpSolverRequest`)                   : containing `CpModelProto` and        :
:                                      : `SatParameters` if needed)           :
| *(new)*                              | `ortools/sat/solution_crush.proto`   |
:                                      : (internal solution-crushing          :
:                                      : representation)                      :

### 2. Removed `SatParameters` Fields

The following fields were removed from `SatParameters`:

Removed v9.15 Field (Tag)                           | v10.0-rc.1 Replacement
:-------------------------------------------------- | :---------------------
`check_drat_proof` (346)                            | `check_lrat_proof` (344) / `check_merged_lrat_proof` (352)
`output_drat_proof` (347)                           | `output_lrat_proof` (345)
`max_drat_time_in_seconds` (348)                    | *(removed; no LRAT timeout parameter)*
`inprocessing_minimization_use_all_orderings` (298) | `num_ordering_in_vivification` (356, default `1`; set `> 1` to try multiple orderings)

> **Note on LRAT proofs**: As of v10.0-rc.1, LRAT proof checking/output supports
> pure SAT models (including multi-worker merged proof verification via
> `check_merged_lrat_proof`). When `cp_model_presolve` is enabled, also set
> `cp_model_pure_sat_presolve = true`, `linearization_level <= 1`,
> `symmetry_level <= 1`, `use_chronological_backtracking = false`, and
> `inprocessing_use_sat_sweeping = false`.

#### C++ (`SatParameters` Removals)

```cpp
// v9.15
SatParameters params;
params.set_check_drat_proof(true);
params.set_output_drat_proof(true);
params.set_max_drat_time_in_seconds(60.0);
params.set_inprocessing_minimization_use_all_orderings(true);
```

```cpp
// v10.0-rc.1
SatParameters params;
params.set_check_lrat_proof(true);
params.set_output_lrat_proof(true);
params.set_cp_model_pure_sat_presolve(true);  // If cp_model_presolve is true
params.set_symmetry_level(1);
params.set_use_chronological_backtracking(false);
params.set_num_ordering_in_vivification(4);
```

#### .NET (C#) (`SatParameters` Removals)

```csharp
// v9.15
CpSolver solver = new CpSolver();
solver.StringParameters =
    "check_drat_proof:true output_drat_proof:true " +
    "inprocessing_minimization_use_all_orderings:true";
```

```csharp
// v10.0-rc.1
CpSolver solver = new CpSolver();
solver.StringParameters =
    "check_lrat_proof:true output_lrat_proof:true " +
    "cp_model_pure_sat_presolve:true symmetry_level:1 " +
    "use_chronological_backtracking:false num_ordering_in_vivification:4";
```

#### Java (`SatParameters` Removals)

```java
// v9.15
CpSolver solver = new CpSolver();
solver.getParameters()
    .setCheckDratProof(true)
    .setOutputDratProof(true)
    .setMaxDratTimeInSeconds(60.0)
    .setInprocessingMinimizationUseAllOrderings(true);
```

```java
// v10.0-rc.1
CpSolver solver = new CpSolver();
solver.getParameters()
    .setCheckLratProof(true)
    .setOutputLratProof(true)
    .setCpModelPureSatPresolve(true)
    .setSymmetryLevel(1)
    .setUseChronologicalBacktracking(false)
    .setNumOrderingInVivification(4);
```

#### Python (`SatParameters` Removals)

```python
# v9.15
solver = cp_model.CpSolver()
solver.parameters.check_drat_proof = True
solver.parameters.output_drat_proof = True
solver.parameters.max_drat_time_in_seconds = 60.0
solver.parameters.inprocessing_minimization_use_all_orderings = True
```

```python
# v10.0-rc.1
solver = cp_model.CpSolver()
solver.parameters.check_lrat_proof = True
solver.parameters.output_lrat_proof = True
solver.parameters.cp_model_pure_sat_presolve = True
solver.parameters.symmetry_level = 1
solver.parameters.use_chronological_backtracking = False
solver.parameters.num_ordering_in_vivification = 4
```

### 3. Changed `SatParameters` Defaults

Even without code changes, note these default value shifts if you rely on exact
solver trajectories or MIP scaling tolerances:

| Parameter                                      | v9.15   | v10.0-rc.1 | Behavioral Impact   |
:                                                : Default : Default    :                     :
| :--------------------------------------------- | :------ | :--------- | :------------------ |
| `use_new_integer_conflict_resolution`          | `false` | **`true`** | Enables the new     |
:                                                :         :            : integer conflict    :
:                                                :         :            : resolution engine   :
:                                                :         :            : whenever the model  :
:                                                :         :            : has at least 10     :
:                                                :         :            : non-binary integer  :
:                                                :         :            : variables.          :
| `shaving_deterministic_time_in_probing_search` | `0.001` | **`0.15`** | Allocates more      |
:                                                :         :            : deterministic       :
:                                                :         :            : effort to bound     :
:                                                :         :            : shaving during      :
:                                                :         :            : probing search.     :
| `mip_check_precision`                          | `1e-4`  | **`1e-5`** | Tighter precision   |
:                                                :         :            : threshold for       :
:                                                :         :            : warning about MIP   :
:                                                :         :            : scaling precision   :
:                                                :         :            : in logs.            :
| `mip_max_valid_magnitude`                      | `1e20`  | **`1e25`** | Allows larger       |
:                                                :         :            : finite              :
:                                                :         :            : variable/constraint :
:                                                :         :            : bound magnitudes    :
:                                                :         :            : before returning    :
:                                                :         :            : `MODEL_INVALID`     :
:                                                :         :            : during MIP scaling. :

### 4. `cp_model.proto` Schema Updates

*   **`InverseConstraintProto`**: Added `f_expr_direct` (field 3) and
    `f_expr_inverse` (field 4) supporting general `LinearExpressionProto` terms
    instead of variable indices only (`f_direct` and `f_inverse` remain
    supported as legacy fields).
*   **`CpSolverResponse`**: Added `int64 inner_objective_value` (field 32),
    reporting the integer objective value of the returned solution in the inner
    (scaled) objective domain alongside `inner_objective_lower_bound`.
*   **`CpObjectiveProto`**: `scaling_was_exact` (field 6) is now marked
    `[deprecated = true]`.
*   **`FloatObjectiveProto`**: Added internal solver fields `domain_mins`
    (field 5) and `domain_maxs` (field 6) — do not set manually.

### 5. Notable New `SatParameters`

The table below highlights the most user-relevant new parameters and enum values
available in v10.0-rc.1:

| Parameter / Enum                                | Default | Purpose                      |
| :---------------------------------------------- | :------ | :--------------------------- |
| `SearchBranching::ROUND_ROBIN_SHAVING_SEARCH`   | —       | Lightweight level-0          |
: (9)                                             :         : round-robin domain/literal   :
:                                                 :         : shaving heuristic.           :
| `SearchBranching::ALT_FIXED_SEARCH` (10)        | —       | Alternates between           |
:                                                 :         : `FIXED_SEARCH` and           :
:                                                 :         : `AUTOMATIC_SEARCH` with      :
:                                                 :         : decaying fixed-search        :
:                                                 :         : probability.                 :
| `cp_model_direct_solve`                         | `false` | Attempts solving the         |
:                                                 :         : unpresolved model in         :
:                                                 :         : parallel with presolve (when :
:                                                 :         : `cp_model_presolve = true`   :
:                                                 :         : and `num_workers > 1`).      :
| `cp_model_pure_sat_presolve`                    | `false` | Simplified pure-SAT presolve |
:                                                 :         : required for LRAT proofs     :
:                                                 :         : when presolve is enabled     :
:                                                 :         : (pure SAT models only).      :
| `try_hint_as_assumptions`                       | `true`  | Tries completing a solution  |
:                                                 :         : hint via assumptions first   :
:                                                 :         : for faster hint              :
:                                                 :         : exploitation.                :
| `use_boolean_equivalences_in_lp`                | `true`  | Substitutes detected Boolean |
:                                                 :         : equivalences in LP           :
:                                                 :         : relaxations for smaller,     :
:                                                 :         : tighter LPs.                 :
| `use_tight_upper_implied_bounds_in_cuts`        | `true`  | Expands tight implied upper  |
:                                                 :         : bounds when generating       :
:                                                 :         : knapsack cover, single-node  :
:                                                 :         : flow, and MIR cuts.          :
| `detect_encoded_complex_domain`                 | `true`  | Presolve rule removing       |
:                                                 :         : unused Boolean encoding      :
:                                                 :         : variables for variables with :
:                                                 :         : complex/gapped domains.      :
| `lns_ignore_redundant_constraints` /            | `true`  | Skips redundant scheduling   |
: `feasibility_jump_ignore_redundant_constraints` :         : relaxation constraints       :
:                                                 :         : during LNS and Feasibility   :
:                                                 :         : Jump.                        :
| `mip_presolve_restrict_bounds`                  | `true`  | Uses dual reasoning to       |
:                                                 :         : restrict variable bounds     :
:                                                 :         : before MIP-to-integer        :
:                                                 :         : scaling.                     :
| `mip_scale_for_precision`                       | `false` | Scales floating-point        |
:                                                 :         : variables by the largest     :
:                                                 :         : power of two satisfying      :
:                                                 :         : `mip_wanted_precision`       :
:                                                 :         : within                       :
:                                                 :         : `mip_max_activity_exponent`. :
| `mip_proper_relaxation`                         | `false` | Ensures discretized MIP      |
:                                                 :         : bounds preserve a valid      :
:                                                 :         : relaxation for lower-bound   :
:                                                 :         : soundness.                   :
| `mip_verify_solution`                           | `false` | Checks returned MIP solution |
:                                                 :         : feasibility against the      :
:                                                 :         : original unpresolved input   :
:                                                 :         : and logs max violation.      :

--------------------------------------------------------------------------------

## C++ Migration Guide

### 1. Headers, Abseil Logging, and `main()` Initialization

Legacy `ortools/base/logging.h` (along with other deprecated `ortools/base/`
compatibility headers such as `numbers.h`, `commandlineflags.h`,
`status_macros.h`, and `int_type.h`) was removed. Include Abseil logging headers
directly and initialize logging in `main()`:

| v9.15 Header                        | v10.0-rc.1 Header                  |
| :---------------------------------- | :--------------------------------- |
| `ortools/base/logging.h`            | `absl/log/log.h` and               |
:                                     : `absl/log/check.h`                 :
| `absl/base/log_severity.h` (in      | `ortools/base/log_severity.h` +    |
: samples)                            : `absl/log/globals.h`               :
| `ortools/sat/boolean_problem.h`     | `ortools/bop/boolean_problem.h`    |
| `ortools/sat/boolean_problem.pb.h`  | `ortools/bop/boolean_problem.pb.h` |
| `ortools/sat/cp_model_service.pb.h` | *(removed)*                        |
| `ortools/sat/drat_writer.h`         | *(removed; use LRAT)*              |

```cpp
// v9.15 (from simple_sat_program.cc)
#include <stdlib.h>

#include "absl/base/log_severity.h"
#include "absl/log/globals.h"
#include "ortools/base/init_google.h"
#include "ortools/base/logging.h"
#include "ortools/sat/cp_model.h"
#include "ortools/sat/cp_model.pb.h"
#include "ortools/sat/cp_model_solver.h"
#include "ortools/util/sorted_interval_list.h"
```

```cpp
// v10.0-rc.1 (from ortools/sat/samples/simple_sat_program.cc)
#include <stdlib.h>

#include "absl/log/check.h"
#include "absl/log/globals.h"
#include "absl/log/log.h"
#include "ortools/base/init_google.h"
#include "ortools/base/log_severity.h"
#include "ortools/sat/cp_model.h"
#include "ortools/sat/cp_model.pb.h"
#include "ortools/sat/cp_model_solver.h"
#include "ortools/util/sorted_interval_list.h"

int main(int argc, char* argv[]) {
  InitGoogle(argv[0], &argc, &argv, true);
  absl::SetStderrThreshold(absl::LogSeverityAtLeast::kInfo);
  operations_research::sat::SimpleSatProgram();
  return EXIT_SUCCESS;
}
```

### 2. `CpModelBuilder` Copy and Move Operators Deleted

Because `IntVar`, `BoolVar`, `IntervalVar`, and `Constraint` keep a raw pointer
to their owning `CpModelBuilder`, copying or moving a `CpModelBuilder` directly
caused dangling pointers. In v10.0-rc.1, `CpModelBuilder`'s copy and move
assignment operators are explicitly `= delete`, and its copy and move
constructors are `private`.

Use `Clone()` to duplicate a model (C++20 guaranteed copy elision allows direct
initialization from `Clone()`), and pass `CpModelBuilder&` by reference instead
of by value:

```cpp
// v9.15 (compiled previously, but risked dangling pointers)
CpModelBuilder base_model;
// ... populate base_model ...
CpModelBuilder copy = base_model;           // Error in v10.0-rc.1
CpModelBuilder moved = std::move(base_model);  // Error in v10.0-rc.1
```

```cpp
// v10.0-rc.1
CpModelBuilder base_model;
// ... populate base_model ...
CpModelBuilder copy = base_model.Clone();
// Retrieve variables on the cloned builder by proto index if needed:
IntVar cloned_x = copy.GetIntVarFromProtoIndex(x.index());
```

### 3. Specialized Constraint Return Types, Fluent Chaining, and New Helpers

Several `CpModelBuilder` methods now return specialized subclasses of
`Constraint` (so existing code storing `Constraint ct = ...` continues to
compile, while `auto` or explicit types can chain incremental additions):

| `CpModelBuilder` Method   | v9.15 Return Type | v10.0-rc.1 Return Type (New |
:                           :                   : Methods)                    :
| :------------------------ | :---------------- | :-------------------------- |
| `AddBoolOr` /             | `Constraint`      | `BoolOrConstraint`          |
: `AddAtLeastOne`           :                   : (`AddLiteral`,              :
:                           :                   : `AddLiterals`)              :
| `AddAtMostOne`            | `Constraint`      | `AtMostOneConstraint`       |
:                           :                   : (`AddLiteral`,              :
:                           :                   : `AddLiterals`)              :
| `AddExactlyOne`           | `Constraint`      | `ExactlyOneConstraint`      |
:                           :                   : (`AddLiteral`,              :
:                           :                   : `AddLiterals`)              :
| `AddBoolAnd`              | `Constraint`      | `BoolAndConstraint`         |
:                           :                   : (`AddLiteral`,              :
:                           :                   : `AddLiterals`)              :
| `AddBoolXor`              | `Constraint`      | `BoolXorConstraint`         |
:                           :                   : (`AddLiteral`,              :
:                           :                   : `AddLiterals`)              :
| `AddNoOverlap(vars = {})` | `Constraint`      | `NoOverlapConstraint`       |
:                           :                   : (`AddInterval`; `vars` now  :
:                           :                   : defaults to `{}`)           :

In addition:

*   **Fluent chaining (`*this`)**: Incremental builders that previously returned
    `void` now return a reference to `*this`: `CircuitConstraint::AddArc`,
    `MultipleCircuitConstraint::AddArc`, `TableConstraint::AddTuple`,
    `ReservoirConstraint::AddEvent` / `AddOptionalEvent`,
    `AutomatonConstraint::AddTransition`, `NoOverlap2DConstraint::AddRectangle`,
    and `CumulativeConstraint::AddDemand`.
*   **`BoolVar` constant checks**: Added `BoolVar::IsFixedToTrue()` and
    `BoolVar::IsFixedToFalse()`.
*   **`Model::Unregister<T>()`**: Added to `ortools/sat/model.h`; `Model::Add`
    and `Model::Get` now accept `const std::function<...>&`.
*   **`SolveWithParameters(model_proto, params)`**: Now unconditionally declared
    in `ortools/sat/cp_model_solver.h` (the
    `#if !defined(__PORTABLE_PLATFORM__)` guard was removed).
*   **Experimental `NewSubsolver(SubsolverFactory)`**: Added to
    `ortools/sat/cp_model_solver.h` for registering custom C++ `SubSolver`
    instances into the parallel search portfolio after presolve.

```cpp
// v10.0-rc.1: fluent specialized constraint builders and BoolVar helpers
CpModelBuilder cp_model;
const BoolVar b1 = cp_model.NewBoolVar();
const BoolVar b2 = cp_model.NewBoolVar();
const BoolVar b3 = cp_model.NewBoolVar();

// Incremental Boolean clauses with chaining:
cp_model.AddBoolOr({b1}).AddLiteral(b2.Not()).AddLiterals({b3});

// Empty-initialized NoOverlap built incrementally:
NoOverlapConstraint no_overlap = cp_model.AddNoOverlap();
no_overlap.AddInterval(task_a).AddInterval(task_b);

// Fluent chaining on TableConstraint / CumulativeConstraint / CircuitConstraint:
cp_model.AddAllowedAssignments({x, y}).AddTuple({0, 1}).AddTuple({1, 0});

if (b1.IsFixedToTrue()) {
  // ...
}

// Experimental custom subsolver registration (ortools/sat/cp_model_solver.h):
Model model;
model.Add(NewSubsolver([](SharedClasses* shared) {
  return std::make_unique<MyCustomSubSolver>(shared);
}));
const CpSolverResponse response = SolveCpModel(cp_model.Build(), &model);
```

### 4. Moved/Removed Headers and Internal Target Renames

*   **`boolean_problem` moved to `bop`**: Update includes from
    `ortools/sat/boolean_problem.h` / `.pb.h` to `ortools/bop/boolean_problem.h`
    / `.pb.h`, namespace from `operations_research::sat` to
    `operations_research::bop`, and Bazel deps from
    `//ortools/sat:boolean_problem{,_cc_proto}` to
    `//ortools/bop:boolean_problem{,_cc_proto}`.
*   **DRAT writer removed**: `ortools/sat/drat_writer.{h,cc}` was removed in
    favor of LRAT (`ortools/sat/lrat_proof_handler.h`,
    `ortools/sat/lrat_checker.h`).
*   **`lrat_checker_main.cc` / `circuit_main.cc`**: Excluded from the CMake
    `ortools` library so linking `libortools` no longer risks duplicate `main()`
    symbols (#5086).
*   **Internal SAT headers & Bazel targets** *(only relevant if you embed
    internal CP-SAT C++ targets directly)*:
    *   `ortools/sat/feasibility_jump.{h,cc}` (`//ortools/sat:feasibility_jump`)
        was renamed to `ortools/sat/violation_ls.{h,cc}`
        (`//ortools/sat:violation_ls`).
    *   `//ortools/sat:sat_base` was split into finer-grained targets
        (`sat_literal`, `sat_assignment`, `sat_clause`, `sat_trail`,
        `sat_status`, with `sat_base` still present).
    *   Constraint-specific presolve rules were split out of
        `//ortools/sat:cp_model_presolve` into
        `//ortools/sat:cp_constraint_presolve`.

### 5. Build Targets (Bazel and CMake)

#### Bazel

Replace `//ortools/base:logging` with `//ortools/base`,
`//ortools/base:log_severity`, and Abseil log targets (and ensure C++20 is
enabled in your toolchain):

```starlark
# v9.15
cc_binary(
    name = "my_sat_app",
    srcs = ["my_sat_app.cc"],
    deps = [
        "//ortools/base:logging",
        "//ortools/sat:cp_model",
        "//ortools/sat:cp_model_cc_proto",
        "//ortools/sat:cp_model_solver",
        "//ortools/util:sorted_interval_list",
    ],
)
```

```starlark
# v10.0-rc.1 (matches ortools/sat/samples/BUILD.bazel)
cc_binary(
    name = "my_sat_app",
    srcs = ["my_sat_app.cc"],
    deps = [
        "//ortools/base",
        "//ortools/base:log_severity",
        "//ortools/sat:cp_model",
        "//ortools/sat:cp_model_cc_proto",
        "//ortools/sat:cp_model_solver",
        "//ortools/util:sorted_interval_list",
        "@abseil-cpp//absl/log",
        "@abseil-cpp//absl/log:check",
        "@abseil-cpp//absl/log:globals",
    ],
)
```

#### CMake

The monolithic `libortools` shared library was split into modular component
libraries (`ortools_core`, `ortools_routing`, `ortools_math_opt`, etc.). CP-SAT
lives inside **`ortools::ortools_core`**; linking the umbrella
`ortools::ortools` target continues to work (it links `ortools_core` publicly),
or you can link `ortools::ortools_core` directly. Make sure your project targets
**C++20** (`CMAKE_CXX_STANDARD 20`) and CMake `>= 3.25`. If building OR-Tools
tests from source, per-language CMake flags (`BUILD_CXX_TESTING`,
`BUILD_PYTHON_TESTING`, `BUILD_JAVA_TESTING`, `BUILD_DOTNET_TESTING`) now
subdivide `BUILD_TESTING`, which remains the main gate (`BUILD_TESTING=ON` is
still required; setting `BUILD_TESTING=OFF` forces all four per-language flags
`OFF`).

--------------------------------------------------------------------------------

## .NET (C#) Migration Guide

There are **no breaking high-level API changes** to `Google.OrTools.Sat`
(`CpModel`, `CpSolver`, `IntVar`, `BoolVar`, `IntervalVar`, `LinearExpr`, or
`Constraint`) between v9.15 and v10.0-rc.1.

### 1. Target Framework and Package Requirements

*   **`.NET 8.0` LTS** is required for official NuGet packages (`net462` and
    `net472` `.NET Framework` targets were dropped from official release
    packages; opt-in CMake flags `USE_DOTNET_462` / `USE_DOTNET_472` remain if
    building from source).
*   **`Google.Protobuf`** dependency bumped to **`3.35.1`**.

### 2. Protobuf and Parameter Updates

*   **`SatParameters` string/proto fields**: Remove `check_drat_proof`,
    `output_drat_proof`, `max_drat_time_in_seconds`, and
    `inprocessing_minimization_use_all_orderings` from any `StringParameters` or
    serialized `SatParameters` (see
    [Protobuf Schema and Search Parameter Changes](#protobuf-schema-and-search-parameter-changes)).
*   **Moved/removed protos**:
    *   `LinearBooleanProblem` / `LinearBooleanConstraint` moved from
        `Google.OrTools.Sat` to **`Google.OrTools.Bop`**.
    *   `CpSolverRequest` (from `cp_model_service.proto`) was removed.
*   **New response/proto properties**: `CpSolverResponse.InnerObjectiveValue`
    and `InverseConstraintProto.FExprDirect` / `FExprInverse` are now available.

### 3. Example Verification Snippet

```csharp
// v10.0-rc.1 (from ortools/sat/samples/SimpleSatProgram.cs)
using System;
using Google.OrTools.Sat;

CpModel model = new CpModel();
IntVar x = model.NewIntVar(0, 2, "x");
IntVar y = model.NewIntVar(0, 2, "y");
model.Add(x != y);

CpSolver solver = new CpSolver();
// Note: use LRAT / num_ordering_in_vivification instead of removed DRAT parameters.
solver.StringParameters = "num_workers:4";
CpSolverStatus status = solver.Solve(model);

if (status == CpSolverStatus.Optimal || status == CpSolverStatus.Feasible) {
  Console.WriteLine($"x = {solver.Value(x)}, y = {solver.Value(y)}");
  Console.WriteLine($"Inner objective: {solver.Response!.InnerObjectiveValue}");
}
```

--------------------------------------------------------------------------------

## Java Migration Guide

### 1. JDK 21+ and Dependency Requirements

*   **JDK 21+** is required (JDK 8, 11, and 17 support was dropped).
*   **`protobuf-java`** and **`protobuf-java-util`** bumped to **`4.35.1`**.
*   Added runtime/compile annotations dependencies (`com.google.guava:guava`,
    `org.jspecify:jspecify:1.0.0`, and Error Prone annotations).

### 2. CP-SAT Protobufs Migrated to SWIG-Generated C++ Wrappers (#5066)

The largest CP-SAT change in Java for v10.0-rc.1 is that all messages and enums
from `cp_model.proto` (`CpModelProto`, `CpSolverResponse`, `ConstraintProto`,
`IntegerVariableProto`, `IntervalConstraintProto`, `CpObjectiveProto`,
`FloatObjectiveProto`, `DecisionStrategyProto`, `CpSolverStatus`, etc.) are now
**SWIG-generated wrappers backed directly by native C++ protobuf objects**
instead of `protobuf-java` `GeneratedMessage` classes. This eliminates expensive
JNI serialization/deserialization overhead when passing large models and
responses between Java and C++.

> **Important**: **`SatParameters`** (`com.google.ortools.sat.SatParameters`)
> **remains a standard `protobuf-java` message**! Only `cp_model.proto` types
> moved to SWIG wrappers.

For code that only uses the high-level `CpModel` and `CpSolver` APIs, the
migration is mostly transparent — **with one critical requirement and several
behavioral differences if you touch `CpModelProto` or `CpSolverResponse`
directly**:

#### A. `Loader.loadNativeLibraries()` Must Be Called Before Any Proto or Model Creation

Because `CpModelProto` and `CpModel` now allocate native C++ protobuf instances
immediately on construction, calling `new CpModel()` or
`CpModelProto.newBuilder()` before `Loader.loadNativeLibraries()` now throws
`UnsatisfiedLinkError` (previously, pure model construction without solving
worked before loading native libraries):

```java
// v10.0-rc.1: ALWAYS load native libraries before constructing CpModel or CpModelProto.
Loader.loadNativeLibraries();
CpModel model = new CpModel();
```

#### B. Key Differences From `protobuf-java` (`GeneratedMessage`)

| Behavior                     | v9.15 (`protobuf-java`)                  | v10.0-rc.1 (SWIG C++ Wrapper)  |
| :--------------------------- | :--------------------------------------- | :----------------------------- |
| **Base class / interfaces**  | Extends                                  | Plain SWIG wrapper class (does |
:                              : `com.google.protobuf.GeneratedMessage`,  : **not** implement              :
:                              : implements `Message` /                   : `com.google.protobuf.Message`) :
:                              : `MessageOrBuilder`                       :                                :
| **`TextFormat` /             | Accepted `CpModelProto` /                | **Not accepted directly**; use |
: `JsonFormat`**               : `CpSolverResponse` directly              : `parseTextFormat(String)`,     :
:                              :                                          : `toString()`, or bridge via    :
:                              :                                          : `toByteArray()` /              :
:                              :                                          : `parseFrom(byte[])`            :
| **`Builder` / `build()` /    | `build()` produced an immutable copy;    | `Builder extends <Message>`;   |
: `toBuilder()`**              : `toBuilder()` cloned                     : `build()` returns `this` and   :
:                              :                                          : `toBuilder()` wraps the        :
:                              :                                          : **same** native C++ pointer    :
:                              :                                          : (no copy!). Use                :
:                              :                                          : `copyFrom(...)` to clone       :
| **`equals()` /               | Structural protobuf value equality       | **Native pointer identity**    |
: `hashCode()`**               :                                          : (`getCPtr(this) ==             :
:                              :                                          : getCPtr(other)`) — do not use  :
:                              :                                          : as value keys in `Map`/`Set`   :
:                              :                                          : or `assertEquals` for deep     :
:                              :                                          : comparison                     :
| **`get*List()` repeated      | Returned unmodifiable `List<T>`          | Returns a **mutable            |
: fields**                     :                                          : `java.util.AbstractList<T>`    :
:                              :                                          : view** directly backed by the  :
:                              :                                          : C++ `RepeatedField`            :
| **Outer descriptor class**   | `com.google.ortools.sat.CpModelProtobuf` | **Removed** (no                |
:                              : generated                                : `CpModelProtobuf` class)       :
| **Enums (`CpSolverStatus`,   | Protobuf Java enum                       | SWIG enum with `getNumber()`,  |
: `DecisionStrategyProto.*`)** :                                          : `forNumber(int)`, and          :
:                              :                                          : `valueOf(String)` preserved    :

#### C. Serialization, Text Format, and Cloning Bridge Patterns

Use the built-in SWIG bridge methods (`toByteArray()`, `parseFrom(byte[])`,
`mergeFrom(byte[])`, `parseTextFormat(String)`, `toString()`, `copyFrom(...)`,
`mergeFrom(...)`, `clear()`):

```java
// v9.15 (protobuf-java APIs that no longer compile on SWIG wrappers)
import com.google.protobuf.TextFormat;

CpModelProto proto = model.model();
CpModelProto copy = proto.toBuilder().build();  // Was a deep copy
boolean same = proto.equals(copy);              // Was structural equality

CpModelProto.Builder parsed = CpModelProto.newBuilder();
TextFormat.merge(textProtoString, parsed);
```

```java
// v10.0-rc.1 (SWIG-generated CpModelProto wrapper APIs)
Loader.loadNativeLibraries();

CpModelProto proto = model.model();

// Deep copy a proto (use copyFrom(), NOT toBuilder().build()):
CpModelProto.Builder copy = CpModelProto.newBuilder();
copy.copyFrom(proto);

// Compare serialized bytes if structural equality is needed:
boolean same = java.util.Arrays.equals(proto.toByteArray(), copy.toByteArray());

// Binary serialization / deserialization:
byte[] bytes = proto.toByteArray();
CpModelProto roundTrip = CpModelProto.parseFrom(bytes);  // Returns null on parse failure

// Textproto parsing and debug formatting:
CpModelProto.Builder fromText = CpModelProto.newBuilder();
boolean ok = fromText.parseTextFormat(textProtoString);
String debugText = fromText.toString();

// File export helper still works as before:
model.exportToFile("model.pb.txt");
```

> **Tip (`JsonFormat` / gRPC interop)**: If your application serializes
> `CpModelProto` or `CpSolverResponse` with `JsonFormat` or sends them over a
> `protobuf-java` gRPC stub, compile `cp_model.proto` into a separate Java
> package in your own build and bridge via `toByteArray()` /
> `parseFrom(byte[])`.

### 3. Moved/Removed Proto Packages and Parameters

*   `boolean_problem.proto` classes moved from `com.google.ortools.sat` to
    **`com.google.ortools.bop`**.
*   `com.google.ortools.sat.v1` (`CpModelServiceProto` / `CpSolverRequest`) was
    removed.
*   Remove calls to `setCheckDratProof`, `setOutputDratProof`,
    `setMaxDratTimeInSeconds`, and `setInprocessingMinimizationUseAllOrderings`
    on `solver.getParameters()`.

### 4. Bazel Targets and Source Layout

If building against OR-Tools with Bazel:

*   Replace `//ortools/sat:cp_model_java_proto` with
    **`//ortools/sat/java:cp_model_proto`**.
*   `//ortools/java/com/google/ortools/sat` now **exports**
    `//ortools/sat/java:cp_model_proto`,
    `//ortools/sat:sat_parameters_java_proto`, and
    `//ortools/util/java:sorted_interval_list`, so depending on
    `//ortools/java/com/google/ortools/sat` (plus
    `//ortools/java/com/google/ortools:Loader`) is sufficient.
*   Repository layout: Java CP-SAT samples moved from `ortools/sat/samples/` to
    `ortools/sat/samples/java/`, and Java tests moved from `ortools/sat/java/`
    to `ortools/javatests/com/google/ortools/sat/`.

```starlark
# v9.15
java_binary(
    name = "MySatApp",
    srcs = ["MySatApp.java"],
    main_class = "com.example.MySatApp",
    deps = [
        "//ortools/java/com/google/ortools:Loader",
        "//ortools/java/com/google/ortools/sat",
        "//ortools/sat:cp_model_java_proto",
    ],
)
```

```starlark
# v10.0-rc.1 (matches ortools/sat/samples/java/BUILD.bazel)
java_binary(
    name = "MySatApp",
    srcs = ["MySatApp.java"],
    main_class = "com.example.MySatApp",
    deps = [
        "//ortools/java/com/google/ortools:Loader",
        "//ortools/java/com/google/ortools/sat",
        "//ortools/sat/java:cp_model_proto",  # Optional: re-exported by //ortools/java/com/google/ortools/sat
    ],
)
```

--------------------------------------------------------------------------------

## Python Migration Guide

### 1. Python Version, Protobuf, and pybind11 Requirements

*   **Python 3.10–3.15** is supported (Python 3.9 was dropped).
*   **`protobuf>=7.35.0,<8.0`** (major bump from `6.33.x`).
*   **`pybind11` v3.1** and `Py_ssize_t` updates under the hood, with
    accelerated `LinearExpr.weighted_sum` and groundwork for free-threaded
    Python (`3.14t` / `3.15t`).

### 2. Legacy CamelCase `DeprecationWarning` Enabled by Default

In v9.15, pre-PEP 8 CamelCase methods on `CpModel` and `CpSolver` were
deprecated, but warnings were silenced unless `cp_model.enable_warnings = True`
was set. In **v10.0-rc.1**, `cp_model.enable_warnings` was **removed** and
**`DeprecationWarning` is emitted by default** on every call to a legacy
CamelCase method (and `warnings.simplefilter("default", DeprecationWarning)` is
applied).

*   Remove any references to `cp_model.enable_warnings` (raises
    `AttributeError`).
*   Migrate legacy CamelCase methods and getter calls to PEP 8 `snake_case`
    methods and properties:

| Legacy CamelCase (Deprecated, Warns  | Recommended PEP 8 (`snake_case` /          |
: in v10.0-rc.1)                       : Property)                                  :
| :----------------------------------- | :----------------------------------------- |
| `model.NewIntVar(...)` /             | `model.new_int_var(...)` /                 |
: `NewBoolVar(...)` /                  : `new_bool_var(...)` / `new_constant(...)`  :
: `NewConstant(...)`                   :                                            :
| `model.NewIntVarFromDomain(...)`     | `model.new_int_var_from_domain(...)`       |
| `model.NewIntervalVar(...)` /        | `model.new_interval_var(...)` /            |
: `NewFixedSizeIntervalVar(...)`       : `new_fixed_size_interval_var(...)`         :
| `model.NewOptionalIntervalVar(...)`  | `model.new_optional_interval_var(...)`     |
| `model.Add(...)` / `AddBoolOr(...)`  | `model.add(...)` / `add_bool_or(...)` /    |
: / `AddBoolAnd(...)` /                : `add_bool_and(...)` /                      :
: `AddAllDifferent(...)`               : `add_all_different(...)`                   :
| `model.AddAtMostOne(...)` /          | `model.add_at_most_one(...)` /             |
: `AddExactlyOne(...)` /               : `add_exactly_one(...)` /                   :
: `AddImplication(...)`                : `add_implication(...)`                     :
| `model.AddNoOverlap(...)` /          | `model.add_no_overlap(...)` /              |
: `AddCumulative(...)` /               : `add_cumulative(...)` / `add_circuit(...)` :
: `AddCircuit(...)`                    :                                            :
| `model.AddHint(...)` /               | `model.add_hint(...)` /                    |
: `AddDecisionStrategy(...)` /         : `add_decision_strategy(...)` /             :
: `AddAssumption(...)`                 : `add_assumption(...)`                      :
| `model.Minimize(...)` /              | `model.minimize(...)` / `maximize(...)` /  |
: `Maximize(...)` / `Clone()` /        : `clone()` / `validate()`                   :
: `Validate()`                         :                                            :
| `model.Name()` / `model.SetName(n)`  | `model.name` / `model.name = n` /          |
: / `model.Proto()`                    : `model.proto` *(properties)*               :
| `constraint.OnlyEnforceIf(...)` /    | `constraint.only_enforce_if(...)` /        |
: `constraint.WithName(...)`           : `constraint.with_name(...)`                :
| `solver.Solve(model, cb)` /          | `solver.solve(model, cb)` /                |
: `solver.Value(e)` /                  : `solver.value(e)` /                        :
: `solver.BooleanValue(lit)`           : `solver.boolean_value(lit)`                :
| `solver.Values(s)` /                 | `solver.values(s)` /                       |
: `solver.BooleanValues(s)` /          : `solver.boolean_values(s)` /               :
: `solver.StatusName()`                : `solver.status_name()`                     :
| `solver.ObjectiveValue()` /          | `solver.objective_value` /                 |
: `solver.BestObjectiveBound()`        : `solver.best_objective_bound`              :
:                                      : *(properties)*                             :
| `solver.WallTime()` /                | `solver.wall_time` / `solver.user_time` /  |
: `solver.UserTime()` /                : `solver.num_conflicts` /                   :
: `solver.NumConflicts()` /            : `solver.num_branches` *(properties)*       :
: `solver.NumBranches()`               :                                            :
| `solver.ResponseProto()` /           | `solver.response_proto` *(property)* /     |
: `solver.ResponseStats()` /           : `solver.response_stats()` /                :
: `solver.SolutionInfo()`              : `solver.solution_info()`                   :
| `solver.SearchForAllSolutions(model, | Set                                        |
: cb)`                                 : `solver.parameters.enumerate_all_solutions :
:                                      : = True` and call `solver.solve(model, cb)` :
| `cp_model.Domain.FromValues(...)` /  | `cp_model.Domain.from_values(...)` /       |
: `FromIntervals(...)` /               : `from_intervals(...)` /                    :
: `FromFlatIntervals(...)`             : `from_flat_intervals(...)`                 :

```python
# v9.15 (silent by default in v9.15; emits DeprecationWarning in v10.0-rc.1)
model = cp_model.CpModel()
model.SetName("example")
x = model.NewIntVar(0, 10, "x")
b = model.NewBoolVar("b")
model.Add(x >= 5).OnlyEnforceIf(b)
model.Maximize(x)

solver = cp_model.CpSolver()
status = solver.Solve(model)
print(solver.StatusName(status), solver.ObjectiveValue(), solver.Value(x), solver.WallTime())
```

```python
# v10.0-rc.1 (recommended PEP 8 API)
model = cp_model.CpModel()
model.name = "example"
x = model.new_int_var(0, 10, "x")
b = model.new_bool_var("b")
model.add(x >= 5).only_enforce_if(b)
model.maximize(x)

solver = cp_model.CpSolver()
status = solver.solve(model)
print(solver.status_name(status), solver.objective_value, solver.value(x), solver.wall_time)
```

### 3. Upfront Validation in `add_reservoir_constraint`

`CpModel.add_reservoir_constraint(...)` now validates arguments immediately when
building the model (matching `add_reservoir_constraint_with_active`) and raises
`ValueError` if any of the following hold:

*   `max_level < min_level`
*   `max_level < 0`
*   `min_level > 0`
*   `times` is empty (`not times`)

Previously, invalid reservoir bounds or empty `times` were only caught later
during model validation or solving (returning `MODEL_INVALID`).

```python
# v10.0-rc.1: raises ValueError immediately if min_level > 0, max_level < 0,
# max_level < min_level, or times is empty.
if times:
    model.add_reservoir_constraint(times, level_changes, min_level=0, max_level=10)
```

### 4. `Constraint.only_enforce_if` Return Values and Stricter Typing (#5568, #5257)

*   **Consistent return value (`self`)**: Previously, some pybind11 overloads of
    `Constraint.only_enforce_if(...)` (and `OnlyEnforceIf(...)`) returned `None`
    instead of `self`, breaking chaining such as
    `model.add(...).only_enforce_if([b1, b2]).with_name("c")`. All overloads now
    return `self` (#5568).
*   **Stricter Boolean literal validation**: Passing an invalid non-Boolean
    integer (such as `ct.only_enforce_if(2)` or `ct.only_enforce_if([2])`) now
    raises `TypeError` explicitly. Valid literal arguments are `BoolVar` /
    `Literal`, `bool`, `0`/`1`, or `numpy.bool_`.
*   **Tightened pybind11 signatures & type hints** (#5257): Overloads on
    `CpModel` and `cp_model_helper` were tightened for static type checkers
    (`mypy`, `pyright`, `pyrefly`).

```python
# v10.0-rc.1: chaining after list/variadic only_enforce_if now works consistently
ct = model.add(x + y <= 10).only_enforce_if([b1, ~b2]).with_name("cap")
```

### 5. `Domain` Type Stubs (`.pyi`) and New Helper Methods

`ortools/util/python/sorted_interval_list.pyi` was added for static type
checking, and `cp_model.Domain` (`sorted_interval_list.Domain`) gained several
new methods:

*   **Static typing note**: In the `.pyi` stub, `Domain.from_intervals` is typed
    as `list[list[int]]` and `Domain.from_values` as `list[int]`. Prefer
    `Domain.from_intervals([[0, 1], [3, 4]])` over `[(0, 1), (3, 4)]` and
    `Domain.from_values([1, 3, 5])` over `{1, 3, 5}` to keep strict type
    checkers happy (see `ortools/sat/samples/step_function_sample_sat.py`).
*   **New `Domain` methods**:
    *   `Domain.empty()` — returns an empty domain
    *   `domain.values()` — returns all integer values in the domain as
        `list[int]`
    *   `domain.value_at_or_before(v)` / `domain.value_at_or_after(v)`
    *   `domain.continuous_multiplication_by(coeff_or_domain)`
    *   `domain.division_by(coeff)` / `domain.division_by_superset(divisor)`
    *   `domain.positive_modulo_by_superset(modulo)`
    *   `__eq__` (`d1 == d2` now performs value equality on domains)
*   **New `cp_model_helper` utility**:
    `cp_model_helper.prettyprint_model_proto(model.proto)` formats a
    `CpModelProto` into a compact, human-readable string.

```python
# v10.0-rc.1
from ortools.sat.python import cp_model
from ortools.sat.python import cp_model_helper as cmh

d1 = cp_model.Domain.from_intervals([[0, 1], [3, 5]])
d2 = cp_model.Domain.from_values([0, 1, 3, 4, 5])
assert d1 == d2
assert d1.values() == [0, 1, 3, 4, 5]
assert d1.value_at_or_after(2) == 3

print(cmh.prettyprint_model_proto(model.proto))
```

### 6. Fixed v9.15 Regressions

Two v9.15 Python regressions are resolved in v10.0-rc.1:

*   **`CpSolver.status_name()`** (#4985): Calling `solver.status_name()` with no
    arguments now works properly (v9.15 raised `TypeError` because it called
    `status()` instead of reading the `status` property).
*   **`CpSolver.Solve(..., solution_callback=...)`** (#4988): Restored the
    `solution_callback` keyword argument name on the legacy `Solve` wrapper
    (which had been accidentally renamed to `callback` in v9.15). Prefer
    `solver.solve(model, solution_callback)` going forward.

### 7. End-to-End Python Example

#### v9.15

```python
from ortools.sat.python import cp_model


def main():
    model = cp_model.CpModel()
    model.SetName("simple_migration_example")

    x = model.NewIntVarFromDomain(
        cp_model.Domain.FromIntervals([(0, 2), (5, 8)]), "x"
    )
    y = model.NewIntVar(0, 5, "y")
    active = model.NewBoolVar("active")

    ct = model.Add(x + y <= 10)
    ct.OnlyEnforceIf([active])  # Returned None in v9.15 for list overloads
    model.Maximize(x + 2 * y)

    solver = cp_model.CpSolver()
    solver.parameters.num_workers = 4
    status = solver.Solve(model)

    if status in (cp_model.OPTIMAL, cp_model.FEASIBLE):
        print(f"Status: {solver.StatusName(status)}")
        print(f"Objective: {solver.ObjectiveValue()}")
        print(f"x = {solver.Value(x)}, y = {solver.Value(y)}")
        print(f"Wall time: {solver.WallTime():.3f}s")
```

#### v10.0-rc.1

```python
from ortools.sat.python import cp_model


def main() -> None:
    model = cp_model.CpModel()
    model.name = "simple_migration_example"

    x = model.new_int_var_from_domain(
        cp_model.Domain.from_intervals([[0, 2], [5, 8]]), "x"
    )
    y = model.new_int_var(0, 5, "y")
    active = model.new_bool_var("active")

    model.add(x + y <= 10).only_enforce_if([active]).with_name("cap")
    model.maximize(x + 2 * y)

    solver = cp_model.CpSolver()
    solver.parameters.num_workers = 4
    status = solver.solve(model)

    if status in (cp_model.OPTIMAL, cp_model.FEASIBLE):
        print(f"Status: {solver.status_name()}")
        print(f"Objective: {solver.objective_value}")
        print(f"x = {solver.value(x)}, y = {solver.value(y)}")
        print(f"Wall time: {solver.wall_time:.3f}s")
```

--------------------------------------------------------------------------------

## References

*   Full updated CP-SAT samples in C++, .NET, Java, and Python:
    [`ortools/sat/samples/`](../samples/) (Java samples in
    [`ortools/sat/samples/java/`](../samples/java/))
*   CP-SAT modeling recipes and troubleshooting: [README.md](README.md),
    [solver.md](solver.md), [troubleshooting.md](troubleshooting.md)
*   Routing solver migration guide:
    [`ortools/routing/docs/migration.md`](../../routing/docs/migration.md)
*   Full v10.0-rc.1 release notes:
    [`docs/CHANGELOG.md`](../../../docs/CHANGELOG.md)
