# v10.0.RC1 What's Changed

## Platforms

*   **C++20 is now required** across all platforms (GCC >= 10, MSVC >= 2022;
    C++17 dropped on Linux/macOS) (#4960).
*   **Python**:
    *   Add Python 3.15 wheel support (manylinux, musllinux, macOS, Windows).
    *   Drop Python 3.9 (EOL); minimum supported version is now Python 3.10.
    *   Free-threaded Python (`3.14t` / `3.15t`) build targets added to Docker
        tooling (wheels are not published yet).
*   **Java**:
    *   **JDK 21 is now required**; drop JDK 8, 11, and 17 support
        (#4961, #4965, #4966).
*   **.NET**:
    *   Default TFM remains `.NET 8.0` LTS; drop `.NET Framework`
        (`net462`/`net472`) from official release NuGet packages (opt-in CMake
        flags `USE_DOTNET_462`/`USE_DOTNET_472`/`USE_DOTNET_9` remain
        available).
*   **Linux distributions**:
    *   Add AlmaLinux 10, RockyLinux 10, Debian 13 (Trixie), Fedora 43, Fedora
        44, and Ubuntu 26.04 LTS support.
    *   Drop AlmaLinux 9, RockyLinux 9, Debian 11 (Bullseye), Fedora 40/41/42,
        Ubuntu 20.04 LTS, and Ubuntu 24.10 support.
    *   Debian 12 (Bookworm) Java release archives are excluded due to JDK 21
        requirement.
*   **Build tools**:
    *   Minimum CMake version bumped from `3.24` to `3.25`.
    *   Bazel pinned to `8.7.0` via `.bazelversion`.
    *   Legacy `Makefile` builds removed from release tooling and documentation.
    *   Emscripten/WebAssembly builds auto-disable FlatZinc and CLI binaries.

## Dependencies

### C++

*   `ZLIB=v1.3.2` (Windows DLL renamed from `zlib1.dll` to `z.dll`).
*   `abseil-cpp=20260817.0`
*   `Protobuf=v35.1`
*   `HiGHS=v1.15.1` (#5024, #5126)
*   `SCIP=v10.0.3`
*   `SoPlex=v8.0.3` (built with `BOOST=OFF`)
*   **Boost removed** as a build dependency (`BUILD_Boost` removed).
*   `RE2=2025-11-05.bcr.1` (Bazel) / `2025-08-12` (CMake).
*   `Eigen=3.4.0` (fetched from GitHub mirror `eigen-mirror/eigen`, #5414).
*   `googletest=1.18.0` (Bazel) / `v1.17.0` (CMake).
*   `protobuf-matchers=v0.1.2`
*   `benchmark=v1.9.5`
*   `SWIG=4.5.1` (CMake/Docker, Java/.NET only) / `4.3.0` (Bazel).
*   Added Gurobi `13.0.1` dynamic loading support and updated `gurobi/optimizer`
    Docker library search paths (#5030).

### Python

*   **SWIG is completely removed from Python** (#5517); all Python wrappers now
    use `pybind11` (`v3.1.0` in CMake / `pybind11_bazel=3.0.1` in Bazel).
*   `protobuf>=7.35.0,<7.36` (major bump from `6.33.x`).

### Java

*   `JDK 21+` required.
*   `protobuf-java=4.35.1` and `protobuf-java-util=4.35.1`.
*   Added `com.google.guava:guava`, `org.jspecify:jspecify:1.0.0`, and Error
    Prone annotations.

### .NET

*   `Google.Protobuf=3.35.1`.

## Breaking Changes

OR-Tools `v10.0` introduces major architectural and API cleanups across all
languages:

*   **Routing split out of `constraint_solver`**:
    *   **C++**: Headers moved from `ortools/constraint_solver/routing*.h` to
        `ortools/routing/*.h`, namespace moved to
        `operations_research::routing`, and main classes drop the `Routing`
        prefix (`routing::Model`, `routing::Dimension`, `routing::IndexManager`,
        `routing::SearchStats`). Protos moved to
        `ortools/routing/{enums,heuristic_parameters,ils,parameters}.proto`
        under package `operations_research.routing`.
    *   **Python**: `ortools.constraint_solver.pywrapcp` is removed and replaced
        by two PEP 8 `snake_case` pybind11 modules:
        `ortools.routing.python.routing` and
        `ortools.constraint_solver.python.constraint_solver`. See
        `ortools/routing/docs/migration.md`.
    *   **Java**: Routing classes moved from
        `com.google.ortools.constraintsolver` to `com.google.ortools.routing`
        (`IndexManager`, `Model`, `Dimension`, `Globals`); CP global helpers
        moved from `main` to `com.google.ortools.constraintsolver.Globals`.
    *   **.NET**: Routing classes moved from `Google.OrTools.ConstraintSolver`
        to `Google.OrTools.Routing` (`IndexManager`, `Model`, `Dimension`,
        `RoutingGlobals`); CP global helpers moved to `ConstraintSolverGlobals`.
*   **Python `pywraplp` migrated to pybind11**: Import path moved from
    `ortools.linear_solver.pywraplp` to `ortools.linear_solver.python.pywraplp`
    (SWIG wrapper and `linear_solver_natural_api.py` removed).
*   **Modular C++ shared libraries**: Monolithic `libortools` split into
    component shared libraries (`ortools_core`, `ortools_math_opt`,
    `ortools_packing`, `ortools_routing`, `ortools_scheduling`, plus umbrella
    `ortools`); removed `BUILD_GLOP` and `BUILD_CXX` CMake options.
*   **Constraint Solver C++ header split**: Removed `constraint_solveri.h` and
    `DemonProfiler`; moved `Assignment`, `Pack`, `IntervalVar`, `SequenceVar`,
    `ModelVisitor`, and `ReversibleEngine` into dedicated headers;
    `ConstraintSolverParameters.TrailCompression::COMPRESS_WITH_ZLIB` enum value
    shifted from `1` to `2`.
*   **CP-SAT & BOP**: Moved `boolean_problem.{h,cc,proto}` from `ortools/sat/`
    to `ortools/bop/` (package `operations_research.bop`), removed DRAT proof
    parameters/writer (replaced by LRAT), deleted `cp_model_service.proto`,
    enabled Python deprecation warnings by default, and migrated Java CP-SAT
    protos to SWIG-generated wrappers (#5066).
*   **MathOpt**: Removed `SolveResultProto.osqp_output` and Python/Java bindings
    (#5421), removed deprecated `SolveStatsProto` fields (`best_primal_bound`,
    `best_dual_bound`, `problem_status`), replaced
    `XpressInitializerProto.extract_names` with `license`, changed
    `XpressParameters::FromProto` to return `absl::StatusOr`, moved
    `StatusProto` to `ortools/util/status.proto`, and added `*Protobuf` Java
    outer class names.
*   **Linear Solver & Glop & PDLP**: Removed
    `KNAPSACK_MIXED_INTEGER_PROGRAMMING` from `MPSolver`; simplified
    `proto_solver` C++ functions to return `MPSolutionResponse` directly;
    replaced `glop::ProblemStatus` with `glop::SolveStatus` in `LPSolver` and
    `RevisedSimplex`; removed deprecated `glop::MPSReader`; removed deprecated
    PDLP `eps_optimal_absolute`/`eps_optimal_relative`.
*   **Graph & Set Cover**: `StaticGraph` and `FlowGraph` are now immutable and
    constructed via `::Builder`; renamed `ortools/graph/graph_io.h` to
    `ortools/graph/io.h`; removed legacy `MinCostFlow` flags/setters; refactored
    Set Cover `SolutionGenerator` classes and `NextSolution` into
    `SetCoverOptimizer` and `Optimize`, migrating `SetCoverMip` to MathOpt.
*   **Core/Base headers**: Removed legacy `ortools/base/` compatibility headers
    (`numbers.h` #5512, `logging.h`, `commandlineflags.h`, `status_macros.h`,
    `source_location.h`, `int_type.h`, `linked_hash_map.h`, `strtoint.h`,
    `array.h`, `sysinfo.h`, `bitmap.h`, `port/os.h`, `util/zvector.h`) in favor
    of Abseil and C++20 equivalents.

## Routing

*   Split Routing out of `ortools/constraint_solver/` into standalone
    `ortools/routing/` across C++, Protobufs, Python, Java, and .NET, with a
    Python migration guide at `ortools/routing/docs/migration.md`.
*   Migrated Python bindings from SWIG (`pywrapcp`) to pybind11
    (`ortools.routing.python.routing`) with native `pybind11_protobuf` support
    and PEP 8 `snake_case` API.
*   Added `FourierSolver` (Fourier-Motzkin parametric LP elimination) and
    SIMD-friendly `MaxLinearExpressionEvaluator`, integrated lazily into
    `PathCumulFilter` for fast start/end/duration bound evaluation.
*   Added generalized disjunctions supporting hard and soft min/max cardinality
    (`MakeDisjunction`, `SetDisjunctionHard{Minimum,Maximum}`,
    `SetDisjunctionSoft{Minimum,Maximum}`), including CP-SAT routing translation
    support.
*   Added variable transits (`Dimension::VariableTransitVar`) and transit target
    scheduling (`OptimizeCumulsOfDimensionFromAssignmentWithTransitTargetInfo`)
    for traffic-aware travel delays separate from break-interruptible slack.
*   Major Iterated Local Search (ILS) expansion:
    *   Added `AdaptiveRandomWalkRuinStrategy`, Late Acceptance Hill-Climbing
        (`late_acceptance_window`),
        `ThresholdAcceptingDeterministicAnnealingAcceptanceStrategy`,
        `RecordToRecordTravelDeterministicAnnealingAcceptanceStrategy`,
        composable `AcceptancePolicy`, `IteratedLocalSearchEventSubscriber`, and
        ILS parameter validation.
    *   Removed `RoutingSearchParameters.use_iterated_local_search` (enabled
        automatically when `iterated_local_search_parameters` is populated) and
        replaced `*_acceptance_strategy` with `*_acceptance_policy`; renamed
        `DefaultIteratedLocalSearchParameters` to
        `RecommendedIteratedLocalSearchParameters`.
*   Extracted `PathState` into a standalone library and migrated
    `PathCumulFilter`, `RouteConstraintFilter`, `VehicleAmortizedCostFilter`,
    `TypeRegulationsFilter`, and `DimensionFilter` to zero-copy `PathState` /
    range-based implementations.
*   Allowed `SCHEDULING_CP_SAT` as
    `RoutingSearchParameters.continuous_scheduling_solver`.
*   Added partial-route node insertion
    (`Model::InsertNodesInAssignmentWithParameters`), resource fixed costs
    (`ResourceGroup::Attributes::fixed_cost`), and ordered vehicle break
    intervals (`Dimension::SetBreakIntervalsOrderingOfVehicle`).
*   Changed `Model::AddRouteConstraint` callback and `Model::GetRouteCost` to
    take `absl::Span<const int64_t>` and exposed `AddRouteConstraint` /
    `GetRouteCost` in C# (#5255).
*   Fixed `SetAllowedVehiclesForIndex` across Python, Java, and .NET wrappers
    (fixes v9.15 known issue #4982).
*   Fixed pickup-to-delivery cumul limits enforcement in
    `Dimension::CloseModel`, multiple unary transit evaluators with optional
    nodes, `ResourceGroupAssignmentFilter::FinalizeAcceptPath` resynchronization
    crashes, `LPCumulFilter::Accept` `FEASIBLE` handling, ILS interruption &
    finalization, and Python custom operator/filter object lifetime (#5083).
*   Exported 23 C++ routing unit tests and benchmarks.

## Constraint Solver

*   Migrated Python bindings from SWIG (`pywrapcp`) to pybind11
    (`ortools.constraint_solver.python.constraint_solver`) with full trampoline
    support for custom Python `LocalSearchOperator`, `IntVarLocalSearchOperator`
    (#5083), `BaseLns`, `PyConstraint`, `Demon`, `IntVarLocalSearchFilter`,
    `DecisionBuilder`, and `SearchMonitor`.
*   Split `constraint_solver.h` and removed `constraint_solveri.h` and
    `DemonProfiler` (`profile_propagation`/`profile_file`).
*   Added `Solver::SetClock(absl::Clock*)` for deterministic time injection,
    `COMPRESS_WITH_GZIP` trail compression, and path-compatibility checking in
    `Relocate`/`Exchange` operators.
*   Removed deprecated `Solver::MemoryUsage()`, millisecond
    `MakeTimeLimit`/`MakeLimit` overloads, and replaced
    `RegularLimit::AbsoluteSolverDeadline()` with `RemainingTime()`.
*   Optimized `CompoundOperator`, reduced `ReversibleEngine` memory footprint
    via zero-padding `std::bit_cast`, modernized `ModelCache` hashing with
    `absl::flat_hash_map`, and replaced `BitCount32`/`BitCount64` with C++20
    `std::popcount`.
*   Fixed Python 3.14 macOS crash (`SIGTRAP`/`SIGABRT`) during
    `RevBool`/`RevInteger` teardown, integer overflow in CP expression negation,
    `reversible_engine` UB, `s390x`/`riscv64` pointer-size detection (#5082),
    and pybind11 visibility warnings (#5420).
*   Exported 33 C++ constraint solver unit tests and benchmarks.

## CP-SAT

*   Added dedicated scheduling local search subsolver (`ls_scheduling`)
    implementing Nowicki & Smutnicki's tabu search for job-shop/machine
    scheduling.
*   Added `ContinuousProber` and new shaving/probing/branching subsolvers
    (`shaving*`, `probing_no_lp`, `alt_fixed*`, `max_hs_*`, `core_or_*`,
    `ROUND_ROBIN_SHAVING_SEARCH`, `ALT_FIXED_SEARCH`).
*   Added parallel unpresolved direct solving (`cp_model_direct_solve`) running
    concurrently with presolve and formalized solution crushing via
    `solution_crush.proto`.
*   Reimplemented `MaxHS` (`optimize_with_max_hs`) using an inner CP-SAT solver,
    removing its SCIP dependency.
*   Overhauled floating-point objectives and MIP scaling: exact floating-point
    objective values and live bound scaling during search, residual continuous
    component analysis with TUM/2-modular detection, precision-driven
    power-of-two scaling (`mip_scale_for_precision`), dual-reasoning bound
    restriction (`mip_presolve_restrict_bounds`), tight discretization bounds
    (`mip_use_tight_bounds`), and optional post-solve solution verification
    (`mip_verify_solution`).
*   Improved LP relaxations and MIP cuts: Boolean equivalence substitution in LP
    (`use_boolean_equivalences_in_lp`), GUB lifting, Boolean bumping,
    Letchford-Souli knapsack cover cuts, tight upper implied bounds in cuts,
    generalized `lin_max` cuts, and upfront GCD simplification / density capping
    / sparsification in `LinearConstraintManager`.
*   Enabled `use_new_integer_conflict_resolution` by default.
*   Added `InverseConstraintProto` general `LinearExpressionProto` support
    (`f_expr_direct`/`f_expr_inverse`), new linear presolve rules (2-term
    tilting, forced big-M decoupling, knapsack redundancy detection,
    complementary `lin_max`, linking-variable elimination, unused complex-domain
    Boolean elimination), and new 2D packing/scheduling presolve rules
    (`PresolveNoOverlap2DFramed`, `ExpandEncoded2DBinPacking`, splitting
    `cumulative` with precedences).
*   Reworked `ModelCopy` / `DenseModelCopy` for LNS and Local Search with
    adaptive LNS presolve deterministic limits, rate-limited Violation LS
    rebuilds, and optional skipping of redundant scheduling constraints in LNS
    and Feasibility Jump.
*   Extracted `GateCongruenceClosure`, added experimental circuit sweeping and
    multi-ordering vivification, and expanded LRAT proof tracking/checking
    (`lrat_checker_main`, `cp_model_pure_sat_presolve`, merged multi-worker LRAT
    verification); removed DRAT proof support.
*   Reworked Shared Tree search (`SharedTreeEncoder`, per-node lower bound
    sharing, LRAT reason export, sync throttling).
*   C++ API: added experimental `NewSubsolver(SubsolverFactory)`, fluent
    chaining (`*this`) on constraint builders, specialized constraint return
    types (`BoolOrConstraint`, `AtMostOneConstraint`, `ExactlyOneConstraint`,
    `BoolAndConstraint`, `BoolXorConstraint`, `NoOverlapConstraint`),
    `BoolVar::IsFixedTo{True,False}()`, and deleted copy/move assignment on
    `CpModelBuilder` (use `Clone()`).
*   Python API: modularized `cp_model_helper` pybind11 bindings, updated for
    pybind11 v3.1 and `Py_ssize_t`, accelerated `LinearExpr.weighted_sum`,
    tightened type hints/stubs (#5257), added upfront
    `add_reservoir_constraint*` validation, and re-enabled `DeprecationWarning`
    for legacy CamelCase methods.
*   Java API: migrated CP-SAT protos to SWIG-generated C++ proto wrappers to
    avoid serialization overhead (#5066).
*   Bug fixes:
    *   Fixed v9.15 Python known issues `CpSolver.status_name()` (#4985) and
        `CpSolver.Solve()` `solution_callback` keyword argument (#4988).
    *   Fixed `Constraint.only_enforce_if` return values and type hints (#5568),
        exponential domain blowup in `DomainSuperSetOf` (#5550), shaving solver
        deadlock (#5532, #5541), 64-bit integer overflow in cut generators and
        `SumOfAllDiffLowerBounder`, non-determinism in interleaved LNS (#5423),
        `int_div` multi-term divisor validation (#5472), silent loss of
        enforcement literals in presolve (#5294), shaving solver `-1` interval
        handling (#5327) and log spam (#5163), `no_overlap` to `all_diff` toggle
        via `infer_all_diffs` (#5337), non-finite floating-point objective
        scaling, `bool_xor` model copy, scheduling/2D explanation bugs, and
        excluded `lrat_checker_main.cc` from `libortools` (#5086).

## FlatZinc

*   Added native `int_pow`, `ortools_array_var_set_element` (#5104), and mixed
    set-variable/set-constant array support, plus predicate documentation in
    `ortools/flatzinc/predicates.md`.
*   Split `--ortools_mode` into `--minizinc_io_mode` and
    `--minizinc_search_mode` and removed `--force_interleave_search`.
*   Revamped 1/4/8/16-worker FlatZinc search portfolios and updated challenge
    tooling for MiniZinc Challenge 2026.
*   Fixed heap out-of-bounds reads on malformed `.fzn` files (#5228),
    `arg_max`/`arg_min` out-of-range encoding (#5064), inverted `set_search`
    value selection, empty interval domain construction, and Bazel Windows
    builds (#5137).

## MathOpt

*   **New Java API & SWIG/JNI wrapper** (`com.google.ortools.mathopt`)
    supporting local solving, log message callbacks, solver callbacks, and
    thread-safe solve interruption, with a JUnit test suite.
*   **New Min-Cost Flow solver (`SOLVER_TYPE_MIN_COST_FLOW`)** across C++,
    Python, and Java wrapping `SimpleMinCostFlow`.
*   **FICO Xpress enhancements**:
    *   Full callback support (`PRESOLVE`, `SIMPLEX`, `MIP`, `MIP_SOLUTION`,
        `MIP_NODE`, `BARRIER`), lazy constraints, user cuts, heuristic solution
        suggestions (including on `MIP_SOLUTION`), and `CallbackSolutionSource`
        (#5231).
    *   Custom license path support (`XpressInitializerProto.License` /
        `XpressLicenseKey`), Python `SolverType.XPRESS` (#5040) and
        `SolveParameters.xpress`.
    *   Fixed empty integer domains (`ceil(lb) > floor(ub)`), empty branching
        priorities (#5471), and 32-bit Windows calling conventions.
*   Added C++ `SolveExecutor` abstraction (`ortools/math_opt/cpp/executor/`),
    `ComputeInfeasibleSubsystem` RPC messages, and Python parameter parsing
    helpers (`parse_solve_parameters`, `parse_gurobi_parameters`,
    `parse_glpk_parameters`).
*   Added `SolveParameters.threads` support and migrated to HiGHS's new callback
    API (`kCallbackLogging` / `kHighsInterrupt`).
*   Updated Glop MathOpt solver for `glop::SolveStatus` (exact termination limit
    reasons) and dual-ray extraction on Phase I infeasibility; added SCIP 10.0+
    compatibility and deterministic constraint insertion order in GSCIP;
    redacted `GurobiInitializerProto.ISVKey.key` in debug logs; switched storage
    sorting to `AutoRadixSort`.
*   Breaking removals: removed `SolveResultProto.osqp_output` (#5421),
    deprecated `SolveStatsProto` fields, `StrictnessProto`,
    `MATH_OPT_USE_EXPRESSION_COUNTERS`, and moved `StatusProto` to
    `ortools/util/status.proto`.
*   Fixed Protobuf v35 Python compatibility, GSCIP format-string safety, MSVC
    Elemental/init-arguments compilation, CMake install target object-list
    leakage (#5146), and exported extensive `storage/`, `validators/`, `io/`,
    and `labs/` unit tests.

## Linear Solver & Model Builder

*   Migrated Python `pywraplp` from SWIG to pybind11
    (`ortools.linear_solver.python.pywraplp`), reimplementing the natural API in
    C++ and removing `linear_solver.swig` and `linear_solver_natural_api.py`
    (#5517, #5361).
*   Removed `KNAPSACK_MIXED_INTEGER_PROGRAMMING` from `MPSolver` and
    `MPModelRequest`.
*   Added CBC solution hints support (`MPSolver::SetHint` ->
    `CbcModel::setBestSolution`, #5232).
*   Added HiGHS logging callback support (`HighsSolveProto` &
    `ModelBuilder`, #5023), fixed HiGHS `FEASIBLE` solution extraction (#5020),
    warning/limit termination status mapping (#5016), and solution-hint
    out-of-bounds memory bug.
*   Exposed `MPSolver.SetStartingLpBasis` in Python.
*   Added `ortools/linear_solver/iis.proto`, `ExactPresolve` and scaling
    improvements in CP-SAT MIP proto solver, thread-local SCIP stderr error
    capture in `SCIP_TO_STATUS`, `absl::Duration` time tracking across
    `MPSolver` interfaces, and safe `MPSolverResponseStatus` conversion.
*   Clamped `±inf` bounds to `SCIPinfinity()` for SCIP 10.0+ compatibility.
*   Fixed MPS and LP export floating-point precision loss using roundtrip double
    formatting (#5501).
*   **Model Builder**:
    *   Fixed Python `Model.add_enforced_linear_constraint` corrupting regular
        constraints (#5302, #5303) and Java JNI log callback exception safety.
    *   Added zero-copy NumPy fast paths in Python `LinearExpr.weighted_sum()`
        and updated `model_builder_helper` for pybind11 v3.1 and `Py_ssize_t`.
    *   Relocated Java Model Builder samples to
        `ortools/model_builder/samples/`.

## Glop, PDLP & BOP

*   **Glop & LP Data**:
    *   Replaced `glop::ProblemStatus` with structured `glop::SolveStatus` in
        `LPSolver` and `RevisedSimplex`, and added `Preprocessor::Result` /
        `SetRequiresBasis` for basis-free postsolve.
    *   Added Ruiz matrix equilibration scaling (`GlopParameters::RUIZ`), primal
        ray computation from dual simplex, ray scaling and quality checks,
        experimental MIP simplex polishing (`primal_polish`/`dual_polish`), dual
        edge norm warm-start transfer, and per-pass preprocessor toggles.
    *   Made `LinearProgram` and `SparseMatrix` movable, switched
        `glop::Stringify(double)` to exact roundtrip formatting, and removed
        deprecated `glop::MPSReader` and standalone `BUILD_GLOP`.
    *   Fixed underflowed zero steps in primal simplex, dual Phase I breakpoint
        tolerance, `LpScalingHelper::VariableScalingFactorWithSlack`, and
        exported comprehensive Glop & LP Data test/benchmark suites.
*   **PDLP**:
    *   Removed deprecated `TerminationCriteria.eps_optimal_absolute` and
        `eps_optimal_relative` (use `simple_optimality_criteria` or
        `detailed_optimality_criteria`).
    *   Updated preprocessing integration for Glop's `SolveStatus` /
        `Preprocessor::Result`.
*   **BOP**:
    *   Moved `boolean_problem.{h,cc,proto}` and BOP `lp_utils` from
        `ortools/sat/` to `ortools/bop/` (package `operations_research.bop`),
        removed experimental `exploit_symmetry_in_sat_first_solution`, and
        updated for Glop `SolveStatus`.

## Graph, Algorithms & Set Cover

*   **Graph**:
    *   Made `StaticGraph` and `FlowGraph` immutable after construction via
        `::Builder`, deprecated `Build()` on mutable graphs, renamed
        `graph_io.h` to `io.h`, removed `MatrixOrFunction` and legacy
        `MinCostFlow` flags/setters (use `MinCostFlowParams`), and modernized
        graph iterators for C++20.
    *   Added Python pybind11 bindings for DAG shortest paths
        (`ortools.graph.python.dag_shortest_path`, #5320) and declared PEP 703
        free-threading support (`mod_gil_not_used`) in `min_cost_flow`,
        `max_flow`, and `linear_sum_assignment` (#5243).
    *   Added `GenericGraph<NodeT>`, `util::graph::Index<T>`,
        `HashOrTreeContainer`, SCC/cycle helpers, `ConnectedComponentsFinder`
        ordering options, `ReadGraphFile()`, HiGHS MIP matching support in
        `ChristofidesPathSolver` (#5150), and shortest-path documentation
        (`ortools/graph/docs/`).
    *   Optimized `SimpleLinearSumAssignment` for complete bipartite graphs and
        `GenericMaxFlow` / `BoundedDijkstraWrapper` with `Bitset64`; fixed
        `SVector` capacity overflow near `IndexT::max()`,
        `ShortestPathsOnDagWrapper` reset, and `FindCliques` early stop.
*   **Algorithms**:
    *   Added `AutoRadixSort` / `MultikeyRadixSort`
        (`ortools/algorithms/auto_radix_sort.h`) and IEEE-754
        `float_ordinal_bijection.h`.
    *   Modernized `AdjustableKAryHeap` (`StrongInt`, `absl::Span`, bug fix in
        two-span `Load()`), added deterministic time-limit enforcement in
        `GraphSymmetryFinder`, and conditionally linked `KnapsackSolver`
        backends in Bazel.
*   **Set Cover**:
    *   Refactored `SolutionGenerator` / `NextSolution` into
        `SetCoverOptimizer<ParamsType>` / `Optimize` (C++ and Python), moved
        CFT/submodel out of `operations_research::scp`, removed
        `CompressedStrongVector` and `Int128` proto fingerprints, and migrated
        `SetCoverMip` from `MPSolver` to MathOpt (with LP relaxation support).
    *   Added `LazyGreedySolutionOptimizer`, `CliqueGuidedLNS`,
        `DualAscentOptimizer`, `SetCoverCftOptimizer`, unified file I/O
        (`ReadModel`/`WriteModel`/`ReadSolution`/`WriteSolution`), reporting
        utilities, and expanded Python bindings.
    *   Fixed out-of-bounds access in set cover file readers (#5260), swapped
        Python `GuidedTabuSearch` Lagrangian getter/setter bindings, and
        duplicate elements in random model generation.

## Build & Dependencies

*   Split monolithic `ortools` C++ shared library into modular component
    libraries (`ortools_core`, `ortools_math_opt`, `ortools_packing`,
    `ortools_routing`, `ortools_scheduling`) with updated Windows DLL exports
    (`OR_DLL`, `OR_INIT_DLL`, `OR_ROUTING_DLL`) and `CPack` packaging (#5544).
*   Unified CMake and Bazel compiler/linker flags via `ortools/copts/copts.py`,
    re-enabling stricter compiler warnings (`-Wmissing-declarations`,
    `-Wcast-qual`, etc.) (#5410, #5511).
*   Added per-language CMake test/benchmark options
    (`BUILD_{CXX,PYTHON,JAVA,DOTNET}_TESTING`, `RUN_BENCHMARKS`), automatic
    `sccache`/`ccache` discovery (#5168), and consistent CMake export names
    (#5281).
*   Modernized Bazel: pinned Bazel `8.7.0`, enabled `bazel build //...` /
    `bazel test //...` (#4963), dropped `rules_proto`, added `rules_jni` for
    macOS JNI linking (#5115), replaced `git_override` with cacheable
    `archive_override`, and upgraded core Bzlmod rules (`rules_cc 0.2.25`,
    `rules_java 9.6.1`, `rules_python 2.2.0`, `rules_go 0.59.0`,
    `gazelle 0.47.0`).
*   Added end-to-end GitHub Actions release pipeline
    (`.github/workflows/release.yml`), unified Unix release scripts, migrated
    Windows CI/release builds to `Ninja` + MSVC + `sccache`, added configurable
    RC versioning, and retargeted Dependabot to `main`.
*   Reorganized Java sources under `ortools/java/com/google/ortools/` and tests
    under `ortools/javatests/com/google/ortools/`; upgraded Maven plugins for
    JDK 21 and Maven Central bundle publishing.

## Other

*   **Core & Utilities**:
    *   Added Python pybind11 bindings for `PiecewiseLinearFunction`
        (`ortools.util.python.piecewise_linear_function`), expanded
        `sorted_interval_list.Domain` Python bindings with `.pyi` type stubs,
        and added `ortools/util/java` (`com.google.ortools.util`) and Go
        `statusstreaming` (#5266).
    *   Added `full_precision_inequalities.h`, `mp_model_solution_checker.h`,
        cross-language `ortools/util/status.proto` & status streaming,
        `BZip2FileReader`, `safe_hash_map`, `UniqueArray`, `TimeLimit::State`,
        `Domain::UniqueValueNotIn`, and moved `deterministic_time.h` to
        `ortools/util/`.
    *   Replaced inline x86_64 assembly in `saturated_arithmetic.h` with
        compiler overflow builtins; migrated status, logging, flags, and number
        parsing to Abseil (#5512); fixed Protobuf `Duration`
        validation/overflow, `PySolveInterrupter::AddTriggerTarget`,
        `RecordWriter`/`RecordReader` error checks, `_WIN32` math defines
        (#5009), and removed `pkg_resources` from `tools/check_python_deps.py`
        (#5319).
*   **Julia**:
    *   Updated `ORTools.jl` to `0.1.1`, `ORToolsBinaries.jl` to `0.1.0`
        (#5109), and `ORToolsGenerated.jl` to `0.1.0` (#5110); refactored binary
        loading to Julia package extensions (`[weakdeps]`), substantially
        expanded MathOptInterface CP-SAT/MathOpt wrappers, fixed Julia v1.13
        `ccall` compatibility and `MOI.supports` signature (#5219).
*   **Scheduling & Packing**:
    *   Added compressed file extension detection (`.bz2`/`.gz`/`.xz`) in
        `JsspParser::ParseFile`, corrected C#/Java proto namespaces, fixed
        `SolveVectorBinPackingWithArcFlow` time limit handling, and exported
        packing unit tests.
*   **Examples & Notebooks**:
    *   Added CP-SAT Tree Ensemble Optimization example & notebook (#5242) and
        new pybind11 `cvrptw.py` sample/notebook.
    *   Dispatched `examples/java/` into per-component
        `ortools/<component>/samples/java/` folders (#4977), moved routing
        samples/notebooks to `ortools/routing/samples/` and
        `examples/notebook/routing/`, removed BSD-3-Clause Java contrib files
        (#5405) and deprecated examples (#4974, #4975), migrated all Python
        examples and regenerated all 266 Jupyter notebooks for v10.0 APIs.

## Changes (PR)

*(Community, external, and automated dependency PRs merged since `v9.15`; 359
PRs total including maintainer PRs)*

*   Use _WIN32 instead of _MSC_VER for _USE_MATH_DEFINES by @jrklasen in
    https://github.com/google/or-tools/pull/5009
*   linear_solver: Fix highs feasible solution extract by @kiijeonghoo in
    https://github.com/google/or-tools/pull/5020
*   cmake: update highs to v1.13.0 by @galabovaa in
    https://github.com/google/or-tools/pull/5024
*   linear_solver: implement logcallback for HiGHS by @kiijeonghoo in
    https://github.com/google/or-tools/pull/5023
*   Add XPRESS to Python MathOpt SolverType enum by @brunovieira-or in
    https://github.com/google/or-tools/pull/5040
*   [CP-SAT] Exclude lrat_checker_main.cc from libortools by @ozan-cb in
    https://github.com/google/or-tools/pull/5086
*   generic test of pointer size in model_cache by @tbrk in
    https://github.com/google/or-tools/pull/5082
*   build(deps): bump the github-actions group with 2 updates by
    @dependabot[bot] in https://github.com/google/or-tools/pull/5096
*   build(deps): bump bazel-contrib/setup-bazel from 0.18.0 to 0.19.0 in the
    github-actions group by @dependabot[bot] in
    https://github.com/google/or-tools/pull/5099
*   Update ORToolsBinaries to 0.1.0 by @dourouc05 in
    https://github.com/google/or-tools/pull/5109
*   Update Julia generated protobuf code by @dourouc05 in
    https://github.com/google/or-tools/pull/5110
*   cmake: update highs to v1.14.0 by @galabovaa in
    https://github.com/google/or-tools/pull/5126
*   Fix Bazel build of FlatZinc on Windows by @cyderize in
    https://github.com/google/or-tools/pull/5137
*   cp-sat: fix typos and improve docstrings in cp_model.py by @chripiermarini
    in https://github.com/google/or-tools/pull/5149
*   Integrate HiGHS and few minor fixes from Augmenta by @PedramHooshangiAUGAI
    in https://github.com/google/or-tools/pull/5150
*   Fix signature of supports for CPSAT Optimizer by @blegat in
    https://github.com/google/or-tools/pull/5219
*   Fix formatting in scheduling docs for interval variables by @ClaytonPassmore
    in https://github.com/google/or-tools/pull/5225
*   build(deps): bump the github-actions group with 2 updates by
    @dependabot[bot] in https://github.com/google/or-tools/pull/5235
*   examples/python: add tree ensemble global optimisation via CP-SAT by
    @adrianhtt in https://github.com/google/or-tools/pull/5242
*   Expose `AddRouteConstraint` to C# / bring back swig to C# by @mslazynski in
    https://github.com/google/or-tools/pull/5255
*   python: declare free-threading support in graph modules by @joshlk in
    https://github.com/google/or-tools/pull/5243
*   build(deps): bump the github-actions group with 2 updates by
    @dependabot[bot] in https://github.com/google/or-tools/pull/5262
*   Validate FlatZinc constraint arity before indexing arguments (heap OOB read)
    by @evilgensec in https://github.com/google/or-tools/pull/5228
*   Route add_enforced_linear_constraint through the enforced helper setters by
    @winklemad in https://github.com/google/or-tools/pull/5303
*   sat: do not remove an enforcement literal from incomplete activity bounds by
    @jipjan in https://github.com/google/or-tools/pull/5294
*   cmake: use consistent export names by @seishun in
    https://github.com/google/or-tools/pull/5281
*   tools: remove pkg_resources from check_python_deps.py by @stephen-huan in
    https://github.com/google/or-tools/pull/5319
*   Skip -1 interval in shaving solver model copy by @z-inform in
    https://github.com/google/or-tools/pull/5327
*   SAT: Allow disabling NoOverlap to AllDiff presolve conversion by @Lian292213
    in https://github.com/google/or-tools/pull/5337
*   build(deps): bump actions/setup-java from 5 to 6 in the github-actions group
    across 1 directory by @dependabot[bot] in
    https://github.com/google/or-tools/pull/5326
*   Implement callback support for Xpress by @djunglas in
    https://github.com/google/or-tools/pull/5231
*   Fix spelling of the in dimacs_assignment comment by @tanvir-ux in
    https://github.com/google/or-tools/pull/5386
*   [CBC interface] Support setting CBC hints by @comick in
    https://github.com/google/or-tools/pull/5232
*   sat: fix non-determinism of interleaved LNS by @bmonastyrsky-t in
    https://github.com/google/or-tools/pull/5423
*   sat: check the whole divisor expression when validating int_div by
    @mvanslobbe in https://github.com/google/or-tools/pull/5472
*   fix(math_opt): skip empty XPRESS branching priorities by @flomnes in
    https://github.com/google/or-tools/pull/5471
*   Preserve precision in MPS model export by @Mr-KnightCLI in
    https://github.com/google/or-tools/pull/5501
*   build(deps): bump bazel-contrib/setup-bazel from 0.19.0 to 0.19.1 in the
    github-actions group by @dependabot[bot] in
    https://github.com/google/or-tools/pull/5515
*   Validate indices in set cover file readers to prevent OOB access by
    @TristanInSec in https://github.com/google/or-tools/pull/5260
*   CP-SAT Python: fix only_enforce_if return values and type hints by
    @hunterhogan in https://github.com/google/or-tools/pull/5568

## New Contributors

*   @jrklasen made their first contribution in
    https://github.com/google/or-tools/pull/5009
*   @kiijeonghoo made their first contribution in
    https://github.com/google/or-tools/pull/5020
*   @brunovieira-or made their first contribution in
    https://github.com/google/or-tools/pull/5040
*   @ozan-cb made their first contribution in
    https://github.com/google/or-tools/pull/5086
*   @tbrk made their first contribution in
    https://github.com/google/or-tools/pull/5082
*   @cyderize made their first contribution in
    https://github.com/google/or-tools/pull/5137
*   @chripiermarini made their first contribution in
    https://github.com/google/or-tools/pull/5149
*   @PedramHooshangiAUGAI made their first contribution in
    https://github.com/google/or-tools/pull/5150
*   @blegat made their first contribution in
    https://github.com/google/or-tools/pull/5219
*   @ClaytonPassmore made their first contribution in
    https://github.com/google/or-tools/pull/5225
*   @adrianhtt made their first contribution in
    https://github.com/google/or-tools/pull/5242
*   @mslazynski made their first contribution in
    https://github.com/google/or-tools/pull/5255
*   @joshlk made their first contribution in
    https://github.com/google/or-tools/pull/5243
*   @evilgensec made their first contribution in
    https://github.com/google/or-tools/pull/5228
*   @winklemad made their first contribution in
    https://github.com/google/or-tools/pull/5303
*   @jipjan made their first contribution in
    https://github.com/google/or-tools/pull/5294
*   @seishun made their first contribution in
    https://github.com/google/or-tools/pull/5281
*   @stephen-huan made their first contribution in
    https://github.com/google/or-tools/pull/5319
*   @z-inform made their first contribution in
    https://github.com/google/or-tools/pull/5327
*   @Lian292213 made their first contribution in
    https://github.com/google/or-tools/pull/5337
*   @tanvir-ux made their first contribution in
    https://github.com/google/or-tools/pull/5386
*   @comick made their first contribution in
    https://github.com/google/or-tools/pull/5232
*   @bmonastyrsky-t made their first contribution in
    https://github.com/google/or-tools/pull/5423
*   @mvanslobbe made their first contribution in
    https://github.com/google/or-tools/pull/5472
*   @Mr-KnightCLI made their first contribution in
    https://github.com/google/or-tools/pull/5501
*   @TristanInSec made their first contribution in
    https://github.com/google/or-tools/pull/5260
*   @hunterhogan made their first contribution in
    https://github.com/google/or-tools/pull/5568

## Known issues

*   macOS 27: Bazel build fails (#5581).
*   Python: Free-threaded (`3.14t`/`3.15t`) wheels are not yet published.

**Full Changelog**: https://github.com/google/or-tools/compare/v9.15...v10.0
