# OR-Tools Subdirectory Dependency Analysis

Using `bazel cquery somepath(kind('cc_library rule', //ortools/a/...), kind('cc_library rule', //ortools/b/...))`to find directory dependencies.

## Name Aliases Table

| Alias | Original Directory |
| --- | --- |
| `Algo` | `ortools/algorithms` |
| `CP` | `ortools/constraint_solver` |
| `Graph` | `ortools/graph` |
| `GraphBase` | `ortools/graph_base` |
| `LS` | `ortools/linear_solver` |
| `MathOpt` | `ortools/math_opt` |
| `SetCovr` | `ortools/set_cover` |
| `3PSolver` | `ortools/third_party_solvers` |

## Dependency Matrix Table

| Source \ Dep | Algo | base | bop | CP | glop | Graph | GraphBase | gurobi | init | LS | lp_data | MathOpt | packing | pdlp | port | routing | sat | scheduling | SetCovr | 3PSolver | util |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| **Algo** | - | Yes | . | . | Yes | Yes | Yes | . | . | Yes | Yes | . | . | . | Yes | . | Yes | . | Yes | . | Yes |
| **base** | . | - | . | . | . | . | . | . | . | . | . | . | . | . | . | . | . | . | . | . | . |
| **bop** | Yes | Yes | - | . | Yes | Yes | Yes | . | . | . | Yes | . | . | . | Yes | . | Yes | . | . | . | Yes |
| **CP** | . | Yes | . | - | . | Yes | . | . | . | . | . | . | . | . | Yes | . | . | . | . | . | Yes |
| **glop** | . | Yes | . | . | - | . | Yes | . | . | . | Yes | . | . | . | Yes | . | . | . | . | . | Yes |
| **Graph** | . | Yes | . | . | . | - | Yes | . | . | Yes | . | . | . | . | Yes | . | . | . | . | . | Yes |
| **GraphBase** | . | Yes | . | . | . | . | - | . | . | . | . | . | . | . | . | . | . | . | . | . | Yes |
| **gurobi** | . | Yes | . | . | . | . | . | - | . | . | . | . | . | . | . | . | . | . | . | Yes | . |
| **init** | Yes | Yes | . | . | Yes | Yes | Yes | . | - | . | Yes | . | . | . | Yes | . | Yes | . | Yes | Yes | Yes |
| **LS** | Yes | Yes | Yes | . | Yes | Yes | Yes | . | . | - | Yes | . | . | Yes | Yes | . | Yes | . | Yes | Yes | Yes |
| **lp_data** | Yes | Yes | . | . | Yes | . | Yes | . | . | . | - | . | . | . | Yes | . | . | . | . | . | Yes |
| **MathOpt** | Yes | Yes | . | . | Yes | Yes | Yes | Yes | . | Yes | Yes | - | . | Yes | Yes | . | Yes | . | Yes | Yes | Yes |
| **packing** | Yes | Yes | Yes | . | Yes | Yes | Yes | . | . | Yes | Yes | . | - | Yes | Yes | . | Yes | . | Yes | Yes | Yes |
| **pdlp** | . | Yes | . | . | Yes | . | Yes | . | . | Yes | Yes | . | . | - | Yes | . | . | . | . | . | Yes |
| **port** | . | Yes | . | . | . | . | . | . | . | . | . | . | . | . | - | . | . | . | . | . | Yes |
| **routing** | Yes | Yes | . | Yes | Yes | Yes | Yes | . | . | Yes | Yes | . | . | . | Yes | - | Yes | . | Yes | . | Yes |
| **sat** | Yes | Yes | . | . | Yes | Yes | Yes | . | . | . | Yes | . | . | . | Yes | . | - | . | Yes | . | Yes |
| **scheduling** | . | Yes | . | . | . | . | . | . | . | Yes | . | . | . | . | Yes | . | . | - | . | . | Yes |
| **SetCovr** | Yes | Yes | . | . | . | . | . | . | . | . | . | . | . | . | Yes | . | . | . | - | . | Yes |
| **3PSolver** | . | Yes | . | . | . | . | . | . | . | . | . | . | . | . | . | . | . | . | . | - | . |
| **util** | . | Yes | . | . | . | . | . | . | . | . | . | . | . | . | Yes | . | . | . | . | . | - |

## Dependency Graph

```mermaid
flowchart LR
  Algo --> base
  Algo --> glop
  Algo --> Graph
  Algo --> GraphBase
  Algo --> LS
  Algo --> lp_data
  Algo --> port
  Algo --> sat
  Algo --> SetCovr
  Algo --> util
  bop --> Algo
  bop --> base
  bop --> glop
  bop --> Graph
  bop --> GraphBase
  bop --> lp_data
  bop --> port
  bop --> sat
  bop --> util
  CP --> base
  CP --> Graph
  CP --> port
  CP --> util
  glop --> base
  glop --> GraphBase
  glop --> lp_data
  glop --> port
  glop --> util
  Graph --> base
  Graph --> GraphBase
  Graph --> LS
  Graph --> port
  Graph --> util
  GraphBase --> base
  GraphBase --> util
  gurobi --> base
  gurobi --> 3PSolver
  init --> Algo
  init --> base
  init --> glop
  init --> Graph
  init --> GraphBase
  init --> lp_data
  init --> port
  init --> sat
  init --> SetCovr
  init --> 3PSolver
  init --> util
  LS --> Algo
  LS --> base
  LS --> bop
  LS --> glop
  LS --> Graph
  LS --> GraphBase
  LS --> lp_data
  LS --> pdlp
  LS --> port
  LS --> sat
  LS --> SetCovr
  LS --> 3PSolver
  LS --> util
  lp_data --> Algo
  lp_data --> base
  lp_data --> glop
  lp_data --> GraphBase
  lp_data --> port
  lp_data --> util
  MathOpt --> Algo
  MathOpt --> base
  MathOpt --> glop
  MathOpt --> Graph
  MathOpt --> GraphBase
  MathOpt --> gurobi
  MathOpt --> LS
  MathOpt --> lp_data
  MathOpt --> pdlp
  MathOpt --> port
  MathOpt --> sat
  MathOpt --> SetCovr
  MathOpt --> 3PSolver
  MathOpt --> util
  packing --> Algo
  packing --> base
  packing --> bop
  packing --> glop
  packing --> Graph
  packing --> GraphBase
  packing --> LS
  packing --> lp_data
  packing --> pdlp
  packing --> port
  packing --> sat
  packing --> SetCovr
  packing --> 3PSolver
  packing --> util
  pdlp --> base
  pdlp --> glop
  pdlp --> GraphBase
  pdlp --> LS
  pdlp --> lp_data
  pdlp --> port
  pdlp --> util
  port --> base
  port --> util
  routing --> Algo
  routing --> base
  routing --> CP
  routing --> glop
  routing --> Graph
  routing --> GraphBase
  routing --> LS
  routing --> lp_data
  routing --> port
  routing --> sat
  routing --> SetCovr
  routing --> util
  sat --> Algo
  sat --> base
  sat --> glop
  sat --> Graph
  sat --> GraphBase
  sat --> lp_data
  sat --> port
  sat --> SetCovr
  sat --> util
  scheduling --> base
  scheduling --> LS
  scheduling --> port
  scheduling --> util
  SetCovr --> Algo
  SetCovr --> base
  SetCovr --> port
  SetCovr --> util
  3PSolver --> base
  util --> base
  util --> port
```

## Directory Pairs Cycles


Cycle: Algo <--> LS
Cycle: Algo <--> lp_data
Cycle: Algo <--> sat
Cycle: Algo <--> SetCovr
Cycle: glop <--> lp_data
Cycle: Graph <--> LS
Cycle: LS <--> pdlp
Cycle: port <--> util

## Strongly Connected Components (Cyclic Dependencies / Clusters)

- **Component 1:** `base` (base)
- **Component 2:** `port` (port), `util` (util)
- **Component 3:** `GraphBase` (graph_base)
- **Component 4:** `3PSolver` (third_party_solvers)
- **Component 5:** `pdlp` (pdlp), `SetCovr` (set_cover), `sat` (sat), `bop` (bop), `LS` (linear_solver), `Graph` (graph), `lp_data` (lp_data), `glop` (glop), `Algo` (algorithms)
- **Component 6:** `CP` (constraint_solver)
- **Component 7:** `gurobi` (gurobi)
- **Component 8:** `init` (init)
- **Component 9:** `MathOpt` (math_opt)
- **Component 10:** `packing` (packing)
- **Component 11:** `routing` (routing)
- **Component 12:** `scheduling` (scheduling)

# Strongly Connected Component (SCC) Condensation Graph

Each node represents an SCC. Multi-directory nodes indicate cyclic dependencies.

```mermaid
flowchart LR
    SCC1["base"]
    SCC2["[port, util]"]
    SCC3["GraphBase"]
    SCC4["3PSolver"]
    SCC5["[pdlp, SetCovr, sat, bop, LS, Graph, lp_data, glop, Algo]"]
    SCC6["CP"]
    SCC7["gurobi"]
    SCC8["init"]
    SCC9["MathOpt"]
    SCC10["packing"]
    SCC11["routing"]
    SCC12["scheduling"]
    SCC10 --> SCC1
    SCC10 --> SCC2
    SCC10 --> SCC3
    SCC10 --> SCC4
    SCC10 --> SCC5
    SCC11 --> SCC1
    SCC11 --> SCC2
    SCC11 --> SCC3
    SCC11 --> SCC5
    SCC11 --> SCC6
    SCC12 --> SCC1
    SCC12 --> SCC2
    SCC12 --> SCC5
    SCC2 --> SCC1
    SCC3 --> SCC1
    SCC3 --> SCC2
    SCC4 --> SCC1
    SCC5 --> SCC1
    SCC5 --> SCC2
    SCC5 --> SCC3
    SCC5 --> SCC4
    SCC6 --> SCC1
    SCC6 --> SCC2
    SCC6 --> SCC5
    SCC7 --> SCC1
    SCC7 --> SCC4
    SCC8 --> SCC1
    SCC8 --> SCC2
    SCC8 --> SCC3
    SCC8 --> SCC4
    SCC8 --> SCC5
    SCC9 --> SCC1
    SCC9 --> SCC2
    SCC9 --> SCC3
    SCC9 --> SCC4
    SCC9 --> SCC5
    SCC9 --> SCC7
```

## Weakly Connected Components (Isolated Module Subgraphs)


note: we expect to found one cluster

- **Cluster 1:** `Algo`, `LS`, `routing`, `packing`, `Graph`, `glop`, `util`, `sat`, `port`, `bop`, `lp_data`, `SetCovr`, `init`, `base`, `GraphBase`, `MathOpt`, `scheduling`, `3PSolver`, `pdlp`, `CP`, `gurobi`

# CMake OR-Tools layout proposal

```mermaid
graph TB
    CORE("libortools_core.so\n(base,port,util,\ngraph,graph_base,algorithms,\nset_cover,sat,glop,bop,pdlp,\nthird_party,lp_data)")
    LS("libortools_linearsolver.so\n(linear_solver)")
    MO("libortools_mathopt.so\n(math_opt,gurobi)")
    PACKING("libortools_packing.so\n(packing)")
    ROUTING("libortools_routing.so\n(routing,constraint_solver)")
    SCHEDULING("libortools_scheduling.so\n(scheduling)")
    OR("libortools.so\n(init)")

    OR --> LS
    LS --> CORE
    OR --> MO
    MO --> CORE
    OR --> PACKING
    PACKING --> CORE
    PACKING --> LS
    OR --> ROUTING
    ROUTING --> CORE
    ROUTING --> LS
    OR --> SCHEDULING
    SCHEDULING --> CORE
    SCHEDULING --> LS
    classDef default stroke:black,fill:lightskyblue,color:black
    linkStyle default color:black
```

