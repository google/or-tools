#!/usr/bin/env python3
# usage: ./tools/check_ortools_deps.py > bazel/docs/README.md
"""Analyzes C++ Bazel dependencies between OR-Tools subdirectories.

This script runs Bazel cquery to analyze top-level module dependency graphs
under ortools/, calculates strongly and weakly connected components, and
outputs Markdown dependency matrices, Mermaid diagrams, and Graphviz DOT graphs.
"""

import collections
import os
import subprocess
import sys

# Subdirectories under ortools/ to exclude from top-level C++ module dependency
# analysis. These fall into the following categories:
# - Non-C++ language bindings and wrappers: dotnet, java, javatests, julia,
#   python
# - Non-code build files and documentation: copts, docs, doxygen, gen
# - Standalone front-ends, sample code, and service endpoints: cpp, flatzinc,
#   model_builder, service
# Note: Core C++ solver components (e.g., constraint_solver, math_opt,
# routing, etc.) are included.
EXCLUDE_DIRS = {
    "copts",
    "cpp",
    "docs",
    "dotnet",
    "doxygen",
    "flatzinc",
    "gen",
    "java",
    "javatests",
    "julia",
    "model_builder",
    "python",
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


def get_ortools_subdirs() -> list[str]:
  """Finds all valid top-level directory names under ortools/.

  Returns:
    A sorted list of valid subdirectory names.
  """
  ortools_path = "ortools"
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
  """Gets the label or alias for a given directory name.

  Args:
    dir_name: Directory name to look up.

  Returns:
    The alias if configured, otherwise the original directory name.
  """
  return DIR_ALIASES.get(dir_name, dir_name)


def cquery_bazel_cc_library_deps(dir_a: str, dir_b: str) -> bool:
  """Checks if //ortools/{dir_a}/... depends on //ortools/{dir_b}/...

  Args:
    dir_a: Source directory name under ortools/.
    dir_b: Target directory name under ortools/.

  Returns:
    True if a dependency path exists.
  """
  target_a = f"kind('cc_library rule', //ortools/{dir_a}/...)"
  target_b = f"kind('cc_library rule', //ortools/{dir_b}/...)"
  cquery = f"somepath({target_a}, {target_b})"
  result = subprocess.run(
      ["bazel", "cquery", cquery], capture_output=True, text=True, check=False
  )
  # If bazel finds at least one path, stdout will contain target names
  return bool(result.stdout.strip())


def find_strongly_connected_components(
    adj_matrix: dict[tuple[str, str], bool], dirs: list[str]
) -> list[list[str]]:
  """Finds Strongly Connected Components (SCCs) using Tarjan's algorithm.

  Args:
    adj_matrix: Dictionary mapping (src, dep) directory pairs to booleans.
    dirs: List of directory names.

  Returns:
    List of strongly connected components, where each component is a list of
    directory names.
  """
  print("Find strongly connected components...", file=sys.stderr)
  index = 0
  indices = {}
  lowlink = {}
  stack = []
  on_stack = set()
  sccs = []

  # Tarjan's algorithm
  # https://en.wikipedia.org/wiki/Tarjan%27s_strongly_connected_components_algorithm#The_algorithm_in_pseudocode
  def strongconnect(curr_node):
    nonlocal index
    indices[curr_node] = index
    lowlink[curr_node] = index
    index += 1
    stack.append(curr_node)
    on_stack.add(curr_node)

    for neighbor in dirs:
      if curr_node == neighbor:
        continue
      if adj_matrix.get((curr_node, neighbor), False):
        if neighbor not in indices:
          strongconnect(neighbor)
          lowlink[curr_node] = min(lowlink[curr_node], lowlink[neighbor])
        elif neighbor in on_stack:
          lowlink[curr_node] = min(lowlink[curr_node], indices[neighbor])

    if lowlink[curr_node] == indices[curr_node]:
      scc = []
      while True:
        w = stack.pop()
        on_stack.remove(w)
        scc.append(w)
        if w == curr_node:
          break
      sccs.append(scc)

  for node in dirs:
    if node not in indices:
      strongconnect(node)
  return sccs


def build_scc_condensation_mermaid(
    adj_matrix: dict[tuple[str, str], bool], sccs: list[list[str]]
) -> str:
  """Builds a Mermaid graph of the SCC condensation.

  Each SCC is represented as a single node, with directed edges between SCCs if
  any underlying directory depends on another.

  Args:
    adj_matrix: Dictionary mapping (src, dep) directory pairs to booleans.
    sccs: List of strongly connected components.

  Returns:
    A string containing the formatted Mermaid diagram markup.
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
    adj_matrix: dict[tuple[str, str], bool], dirs: list[str]
) -> list[list[str]]:
  """Finds Weakly Connected Components (WCCs).

  Uses BFS/DFS on an undirected view of the graph.

  Args:
    adj_matrix: Dictionary mapping (src, dep) directory pairs to booleans.
    dirs: List of directory names.

  Returns:
    List of weakly connected components, where each component is a list of
    directory names.
  """
  print("Find weakly connected components...", file=sys.stderr)
  undirected_adj = collections.defaultdict(set)
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
  """Computes the dependency matrix for all pairs of directories.

  Args:
    dirs: List of directory names to analyze.

  Returns:
    A dictionary mapping (src, dep) directory pairs to booleans indicating
    dependency existence.
  """
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
  """Builds a Markdown table mapping directory aliases to original names.

  Args:
    dirs: List of directory names.

  Returns:
    A string containing the Markdown table of aliases, or empty string if no
    aliases exist.
  """
  print("Generate aliases table...", file=sys.stderr)
  legend_lines = ["| Alias | Original Directory |", "| --- | --- |"]
  has_aliases = False
  for src_dir in dirs:
    if src_dir in DIR_ALIASES:
      legend_lines.append(f"| `{DIR_ALIASES[src_dir]}` | `ortools/{src_dir}` |")
      has_aliases = True
  return "\n".join(legend_lines) if has_aliases else ""


def build_matrix_table(
    dirs: list[str], dep_matrix: dict[tuple[str, str], bool]
) -> str:
  """Builds a Markdown matrix table of directory dependencies.

  Args:
    dirs: List of directory names.
    dep_matrix: Dictionary mapping (src, dep) directory pairs to booleans.

  Returns:
    A string containing the formatted Markdown matrix table.
  """
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


def build_mermaid_diag(dep_matrix: dict[tuple[str, str], bool]) -> str:
  """Builds a Mermaid flowchart diagram representing directory dependencies.

  Args:
    dep_matrix: Dictionary mapping (src, dep) directory pairs to booleans.

  Returns:
    A string containing the formatted Mermaid diagram markup.
  """
  print("Generate mermaid graph...", file=sys.stderr)
  mermaid_lines = ["```mermaid", "flowchart LR"]
  for (src_dir, dep_dir), has_dep in dep_matrix.items():
    if has_dep:
      # Direction: src_dir --> dep_dir (src_dir depends on dep_dir)
      mermaid_lines.append(f"  {get_label(src_dir)} --> {get_label(dep_dir)}")
  mermaid_lines.append("```")
  return "\n".join(mermaid_lines)


def build_dot_diag(dep_matrix: dict[tuple[str, str], bool]) -> str:
  """Builds a Graphviz DOT diagram representing directory dependencies.

  Args:
    dep_matrix: Dictionary mapping (src, dep) directory pairs to booleans.

  Returns:
    A string containing the formatted Graphviz DOT markup.
  """
  print("Generate dot graph...", file=sys.stderr)
  dot_lines = [
      "```",
      "digraph ortools_dependencies {",
      '  rankdir="LR";',
      (
          '  node [shape=box, style="filled,rounded", fillcolor="#f8f9fa",'
          ' fontname="sans-serif"];'
      ),
      '  edge [color="#4a5568", arrowhead="vee"];',
      "",
  ]
  for (src_dir, dep_dir), has_dep in dep_matrix.items():
    if has_dep:
      # Direction: src_dir -> dep_dir (src_dir depends on dep_dir)
      dot_lines.append(f'  "{src_dir}" -> "{dep_dir}";')
  dot_lines.extend(["}", "```"])
  return "\n".join(dot_lines)


def build_directory_pair_cycle(
    dirs: list[str], dep_matrix: dict[tuple[str, str], bool]
) -> str:
  """Finds circular dependencies between pairs of directories.

  Args:
    dirs: List of directory names.
    dep_matrix: Dictionary mapping (src, dep) directory pairs to booleans.

  Returns:
    A string listing 2-cycle directory dependencies formatted in Markdown.
  """
  print("Find directory pairs cycle...", file=sys.stderr)
  cycle_lines = []
  count = 1
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
            f"- **Cycle {count}:** `{get_label(src_dir)}` <-->"
            f" `{get_label(dep_dir)}`"
        )
        count += 1
  return "\n".join(cycle_lines)


def analyze_dependencies(
    dirs: list[str],
) -> tuple[dict[tuple[str, str], bool], list[list[str]], list[list[str]]]:
  """Evaluates dependencies of Bazel cc_library targets for directory pairs.

  Args:
    dirs: List of directory names to analyze.

  Returns:
    A tuple containing:
      - deps_matrix: Dictionary mapping (src, dep) directory pairs to booleans.
      - sccs: List of strongly connected components.
      - wccs: List of weakly connected components.
  """
  deps_matrix = compute_deps_matrix(dirs)
  # fix: remove SetCover -> MathOpt deps (ed set_cover_mip test library)
  deps_matrix[("set_cover", "math_opt")] = False

  # Calculate Components
  sccs = find_strongly_connected_components(deps_matrix, dirs)
  wccs = find_weakly_connected_components(deps_matrix, dirs)

  return (deps_matrix, sccs, wccs)


def main() -> None:
  """Main entry point for analyzing OR-Tools subdirectory dependencies."""
  subdirectories = get_ortools_subdirs()
  print(
      f"Found {len(subdirectories)} subdirectories to check.", file=sys.stderr
  )

  dep_matrix, scc, wcc = analyze_dependencies(subdirectories)

  # Print MD report to stdout
  print("<!-- disclaimer: this is an auto-generated report, do not modify -->")
  print("<!-- disableFinding(LINE_OVER_80) -->")
  print("# OR-Tools Subdirectory Dependency Analysis\n")
  print((
      "Using `bazel cquery somepath(kind('cc_library rule', //ortools/a/...),"
      " kind('cc_library rule', //ortools/b/...))`to find directory"
      " dependencies."
  ))
  print("\n## Name Aliases Table\n")
  aliases_table = build_aliases_table(subdirectories)
  print(aliases_table)
  print("\n## Dependency Matrix Table\n")
  matrix_table = build_matrix_table(subdirectories, dep_matrix)
  print(matrix_table)
  print("\n## Dependency Graph\n")
  mermaid_diagram = build_mermaid_diag(dep_matrix)
  print(mermaid_diagram)
  print("\n## Directory Pairs Cycles\n")
  dir_pair_cycles = build_directory_pair_cycle(subdirectories, dep_matrix)
  print(dir_pair_cycles)

  print("\n## Strongly Connected Components (Cyclic Dependencies / Clusters)\n")
  scc_summary = []
  for idx, comp in enumerate(scc, 1):
    labels = [f"`{get_label(c)}`" for c in comp]
    scc_summary.append(f"- **Component {idx}:** " + ", ".join(sorted(labels)))
  scc_out = "\n".join(scc_summary)
  print(scc_out)

  print("\n## Strongly Connected Component (SCC) Condensation Graph\n")
  print(
      "Each node represents an SCC. Multi-directory nodes indicate cyclic"
      " dependencies.\n"
  )
  mermaid_scc_graph = build_scc_condensation_mermaid(dep_matrix, scc)
  print(mermaid_scc_graph)

  print("\n## Weakly Connected Components (Isolated Module Subgraphs)\n")
  print("note: we expect to found one cluster\n")
  wcc_summary = []
  for idx, comp in enumerate(wcc, 1):
    labels = [f"`{get_label(c)}`" for c in comp]
    wcc_summary.append(f"- **Cluster {idx}:** " + ", ".join(sorted(labels)))
  wcc_out = "\n".join(wcc_summary)
  print(wcc_out)

  print("\n## CMake OR-Tools layout proposal\n")
  diag = """\
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
"""
  print(diag)


if __name__ == "__main__":
  main()
