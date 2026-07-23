#!/usr/bin/env python3
import os
import subprocess
import sys
from collections import defaultdict

# Subdirectories/files in ortools/ to ignore (non-package dirs or build files)
EXCLUDE_DIRS = {
    "BUILD.bazel",
    "BUILD",
    "doxygen",
    "gen",
    "docs",
    "copts",
    #"constraint_solver",  # routing deps
    #"routing",  # leaf
    "cpp",
    "dotnet",
    "flatzinc",
    #"math_opt",  # leaf
    #"gurobi",  # math_opt deps
    #"init",  # leaf
    "java",
    "javatests",
    "julia",
    "model_builder",  # only samples
    #"packing",  # leaf
    #"scheduling",  # leaf
    "python",
    #"third_party_solvers",
    "service",
}

# Map long directory names to short aliases.
# Directories not listed here will retain their original name.
DIR_ALIASES = {
    "algorithms": "Algo",
    "constraint_solver": "CP",
    "graph": "Graph",
    "graph_base": "GraphBase",
    "linear_solver": "LS",
    "math_opt": "MathOpt",
    "model_builder": "ModelBldr",
    "set_cover": "SetCovr",
    "third_party_solvers": "3PSolver",
}


def get_ortools_subdirs(ortools_path: str = "ortools") -> list[str]:
    """Finds all valid top-level directory names under ortools/."""
    if not os.path.exists(ortools_path):
        print(f"Error: Directory '{ortools_path}' not found.", file=sys.stderr)
        sys.exit(1)

    subdirs = [
        d
        for d in os.listdir(ortools_path)
        if os.path.isdir(os.path.join(ortools_path, d)) and d not in EXCLUDE_DIRS
    ]
    return sorted(subdirs)


def get_label(dir_name: str) -> str:
    """Returns the alias if configured, otherwise the original directory name."""
    return DIR_ALIASES.get(dir_name, dir_name)


def cquery_bazel_cc_library_deps(dir_a: str, dir_b: str) -> bool:
    """
    Checks if //ortools/{dir_a}/... depends on //ortools/{dir_b}/...
    Returns True if a dependency path exists.
    """
    target_a = f"kind('cc_library rule', //ortools/{dir_a}/...)"
    target_b = f"kind('cc_library rule', //ortools/{dir_b}/...)"
    cquery = f"somepath({target_a}, {target_b})"
    try:
        result = subprocess.run(
            ["bazel", "cquery", cquery], capture_output=True, text=True, check=False
        )
        # If bazel finds at least one path, stdout will contain target names
        return bool(result.stdout.strip())
    except Exception:
        return False


def find_strongly_connected_components(
    adj_matrix: dict, dirs: list[str]
) -> list[list[str]]:
    """Tarjan's algorithm for finding Strongly Connected Components (SCCs)."""
    print("Find strongly connected components...", file=sys.stderr)
    index = 0
    indices = {}
    lowlink = {}
    stack = []
    on_stack = set()
    sccs = []

    def strongconnect(node):
        nonlocal index
        indices[node] = index
        lowlink[node] = index
        index += 1
        stack.append(node)
        on_stack.add(node)

        for neighbor in dirs:
            if node == neighbor:
                continue
            if adj_matrix.get((node, neighbor), False):
                if neighbor not in indices:
                    strongconnect(neighbor)
                    lowlink[node] = min(lowlink[node], lowlink[neighbor])
                elif neighbor in on_stack:
                    lowlink[node] = min(lowlink[node], indices[neighbor])

        if lowlink[node] == indices[node]:
            scc = []
            while True:
                w = stack.pop()
                on_stack.remove(w)
                scc.append(w)
                if w == node:
                    break
            sccs.append(scc)

    for node in dirs:
        if node not in indices:
            strongconnect(node)
    return sccs


def build_scc_condensation_mermaid(
    adj_matrix: dict, dirs: list[str], sccs: list[list[str]]
) -> str:
    """
    Builds a Mermaid graph where:
    - Each SCC is a single node.
    - Singletons display their regular name/alias.
    - Multi-node SCCs display all member directories.
    - Directed edges exist between SCCs if any underlying directory depends on another.
    """
    # 1. Map each directory to its SCC ID
    dir_to_scc_id = {}
    scc_node_labels = {}
    scc_node_ids = {}

    for idx, scc in enumerate(sccs, start=1):
        scc_id = f"SCC{idx}"
        for d in scc:
            dir_to_scc_id[d] = scc_id

        scc_node_ids[scc_id] = scc_id

        # Label formatting: if singleton use name, else list all members
        if len(scc) == 1:
            scc_node_labels[scc_id] = get_label(scc[0])
        else:
            members_str = ", ".join([get_label(d) for d in scc])
            scc_node_labels[scc_id] = f"[{members_str}]"

    # 2. Build edges between SCCs (avoiding self-loops and duplicate edges)
    scc_edges = set()
    for (src_dir, dep_dir), has_dep in adj_matrix.items():
        if has_dep:
            src_scc = dir_to_scc_id[src_dir]
            dep_scc = dir_to_scc_id[dep_dir]
            if src_scc != dep_scc:
                scc_edges.add((src_scc, dep_scc))

    # 3. Construct Mermaid output
    mermaid_lines = ["```mermaid", "flowchart LR"]

    # Declare nodes with labels
    for scc_id, label in scc_node_labels.items():
        # Double square brackets or rounded boxes for multi-node clusters
        if "[" in label:
            mermaid_lines.append(f'    {scc_id}["{label}"]')
        else:
            mermaid_lines.append(f'    {scc_id}["{label}"]')

    # Add Directed Edges between SCCs
    for src_scc, dep_scc in sorted(scc_edges):
        mermaid_lines.append(f"    {src_scc} --> {dep_scc}")

    mermaid_lines.append("```")
    return "\n".join(mermaid_lines)


def find_weakly_connected_components(
    adj_matrix: dict, dirs: list[str]
) -> list[list[str]]:
    """BFS/DFS on an undirected view of the graph to find WCCs."""
    print("Find weakly connected components...", file=sys.stderr)
    undirected_adj = defaultdict(set)
    for (u, v), has_edge in adj_matrix.items():
        if has_edge:
            undirected_adj[u].add(v)
            undirected_adj[v].add(u)

    visited = set()
    wccs = []
    for node in dirs:
        if node not in visited:
            component = []
            queue = [node]
            visited.add(node)
            while queue:
                curr = queue.pop(0)
                component.append(curr)
                for neighbor in undirected_adj[curr]:
                    if neighbor not in visited:
                        visited.add(neighbor)
                        queue.append(neighbor)
            wccs.append(component)
    return wccs


def compute_deps_matrix(dirs: list[str]) -> dict[tuple[str, str], bool]:
    dep_matrix = {}
    for i, src_dir in enumerate(dirs, start=1):
        print(
            f"[{i}/{len(dirs)}] Checking dependencies for ortools/{src_dir}...",
            file=sys.stderr,
        )
        for dep_dir in dirs:
            if src_dir == dep_dir:
                continue
            dep_matrix[(src_dir, dep_dir)] = cquery_bazel_cc_library_deps(
                src_dir, dep_dir
            )
        # print(f"query:\n{dep_matrix}", file=sys.stderr)
    return dep_matrix


def build_aliases_table(dirs: list[str]) -> str:
    print("Generate aliases table...", file=sys.stderr)
    legend_lines = ["| Alias | Original Directory |", "| --- | --- |"]
    has_aliases = False
    for src_dir in dirs:
        if src_dir in DIR_ALIASES:
            legend_lines.append(f"| `{DIR_ALIASES[src_dir]}` | `ortools/{src_dir}` |")
            has_aliases = True
    return "\n".join(legend_lines) if has_aliases else ""


def build_matrix_table(dirs: list[str], dep_matrix: dict[tuple[str, str], bool]) -> str:
    print("Generate markdown table...", file=sys.stderr)
    headers = ["Source \\ Dep"] + [get_label(d) for d in dirs]
    md_lines = [
        "| " + " | ".join(headers) + " |",
        "| " + " | ".join(["---"] * len(headers)) + " |",
    ]
    for src_dir in dirs:
        row = [f"**{get_label(src_dir)}**"]
        for dep_dir in dirs:
            if src_dir == dep_dir:
                row.append("-")
            else:
                has_dep = dep_matrix[(src_dir, dep_dir)]
                row.append("Yes" if has_dep else ".")
        md_lines.append("| " + " | ".join(row) + " |")
    return "\n".join(md_lines)


def build_mermaid_diag(dirs: list[str], dep_matrix: dict[tuple[str, str], bool]) -> str:
    print("Generate mermaid...", file=sys.stderr)
    mermaid_lines = ["```mermaid", "flowchart LR"]
    for (src_dir, dep_dir), has_dep in dep_matrix.items():
        if has_dep:
            # Direction: src_dir --> dep_dir (src_dir depends on dep_dir)
            mermaid_lines.append(f"  {get_label(src_dir)} --> {get_label(dep_dir)}")
    mermaid_lines.append("```")
    return "\n".join(mermaid_lines)


def build_dot_diag(dirs: list[str], dep_matrix: dict[tuple[str, str], bool]) -> str:
    print("Generate dot graph...", file=sys.stderr)
    dot_lines = [
        "```",
        "digraph ortools_dependencies {",
        '  rankdir="LR";',
        '  node [shape=box, style="filled,rounded", fillcolor="#f8f9fa", fontname="sans-serif"];',
        '  edge [color="#4a5568", arrowhead="vee"];',
        "",
    ]
    for (src_dir, dep_dir), has_dep in dep_matrix.items():
        if has_dep:
            # Direction: src_dir -> dep_dir (src_dir depends on dep_dir)
            dot_lines.append(f'  "{src_dir}" -> "{dep_dir}";')
    dot_lines.extend(["}", "```"])
    return "\n".join(dot_lines)


def build_cycle(dirs: list[str], dep_matrix: dict[tuple[str, str], bool]) -> str:
    print("Find directory pairs cycle...", file=sys.stderr)
    cycle_lines = [""]
    for src_dir in dirs:
        for dep_dir in dirs:
            if src_dir == dep_dir or src_dir > dep_dir:
                continue
            if dep_matrix[(src_dir, dep_dir)] and dep_matrix[(dep_dir, src_dir)]:
                print(
                    f"Find cycle {get_label(src_dir)} <--> {get_label(dep_dir)}",
                    file=sys.stderr,
                )
                cycle_lines.append(
                    f"Cycle: {get_label(src_dir)} <--> {get_label(dep_dir)}"
                )
    return "\n".join(cycle_lines)


def analyze_dependencies(
    dirs: list[str],
) -> tuple[dict[tuple[str, str], bool], list[list[str]], list[list[str]]]:
    """
    Evaluates dependencies of all bazel cc_library for all directory pairs and
    constructs few tables.
    * The deps matrix
    * The strongly connected components
    * The weakly connected components
    """
    deps_matrix = compute_deps_matrix(dirs)
    # fix: remove SetCover -> MathOpt deps (ed set_cover_mip test library)
    deps_matrix[("set_cover", "math_opt")] = False

    # Calculate Components
    sccs = find_strongly_connected_components(deps_matrix, dirs)
    wccs = find_weakly_connected_components(deps_matrix, dirs)

    return (deps_matrix, sccs, wccs)


if __name__ == "__main__":
    subdirectories = get_ortools_subdirs("ortools")
    print(f"Found {len(subdirectories)} subdirectories to check.", file=sys.stderr)

    dep_matrix, scc, wcc = analyze_dependencies(subdirectories)

    # Print MD report to stdout
    print("# OR-Tools Subdirectory Dependency Analysis\n")
    print(
        (
            "Using `bazel cquery somepath(kind('cc_library rule', //ortools/a/...), kind('cc_library rule', //ortools/b/...))`"
            "to find directory dependencies."
        )
    )
    print("\n## Name Aliases Table\n")
    aliases_table = build_aliases_table(subdirectories)
    print(aliases_table)
    print("\n## Dependency Matrix Table\n")
    matrix_table = build_matrix_table(subdirectories, dep_matrix)
    print(matrix_table)
    print("\n## Dependency Graph\n")
    mermaid_diagram = build_mermaid_diag(subdirectories, dep_matrix)
    print(mermaid_diagram)
    print("\n## Directory Pairs Cycles\n")
    cycles = build_cycle(subdirectories, dep_matrix)
    print(cycles)

    print("\n## Strongly Connected Components (Cyclic Dependencies / Clusters)\n")
    scc_summary = []
    for idx, comp in enumerate(scc, 1):
        labels = [f"`{get_label(c)}` ({c})" for c in comp]
        scc_summary.append(f"- **Component {idx}:** " + ", ".join(labels))
    scc_out = "\n".join(scc_summary)
    print(scc_out)

    print("\n# Strongly Connected Component (SCC) Condensation Graph\n")
    print(
        "Each node represents an SCC. Multi-directory nodes indicate cyclic dependencies.\n"
    )
    mermaid_scc_graph = build_scc_condensation_mermaid(dep_matrix, subdirectories, scc)
    print(mermaid_scc_graph)

    print("\n## Weakly Connected Components (Isolated Module Subgraphs)\n")
    print("\nnote: we expect to found one cluster\n")
    wcc_summary = []
    for idx, comp in enumerate(wcc, 1):
        labels = [f"`{get_label(c)}`" for c in comp]
        wcc_summary.append(f"- **Cluster {idx}:** " + ", ".join(labels))
    wcc_out = "\n".join(wcc_summary)
    print(wcc_out)


    print("\n# CMake OR-Tools layout proposal\n")
    diag = '''\
```mermaid
graph TB
    CORE("libortools_core.so\\n(base,port,util,\\ngraph,graph_base,algorithms,\\nset_cover,sat,glop,bop,pdlp,\\nthird_party,lp_data)")
    LS("libortools_linearsolver.so\\n(linear_solver)")
    MO("libortools_mathopt.so\\n(math_opt,gurobi)")
    PACKING("libortools_packing.so\\n(packing)")
    ROUTING("libortools_routing.so\\n(routing,constraint_solver)")
    SCHEDULING("libortools_scheduling.so\\n(scheduling)")
    OR("libortools.so\\n(init)")

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
'''
    print(diag)

