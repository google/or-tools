# Routing

This folder contains utilities for usual file formats useful in routing problems
and utilities directly related to these file formats.

`solution_serializer.h` contains a generic serializer for routing solutions for
many formats.

<!-- mdformat off(go/g3mark doesn't support multiline tables, b/267197123) -->
| Problem type | File format | Corresponding parser | Data storage | Data sets |
| ------------ | ----------- | -------------------- | ------------ | --------- |
| TSP | TSPLIB | `tsplib_parser.h` | | [TSPLIB95][tsplib95] |
| TSPTW | TSPTW | `tsptw_parser.h` | | [TSPTW][tsptw] |
| PDTSP / TSPPD | PDTSP | `pdtsp_parser.h` | | [PDTSP][pdtsp] |
| CVRP | TSPLIB | `tsplib_parser.h` | | [TSPLIB95][tsplib95] |
| VRPTW | Solomon | `solomon_parser.h` | | [Solomon][solomon], [Homberger][homberger] |
| CARP | CARPLIB | `carplib_parser.h` | | [CARPLIB][carplib] |
| NEARP | NEARPLIB | `nearplib_parser.h` | | [NEARPLIB][nearplib] |
| PDPTW | LiLim | `lilim_parser.h` | | [LiLim][lilim] |
| MCND | Dow | `dow_parser.h` | `capacity_planning.proto` | [Canad][canad] |
<!-- mdformat on -->

In the future, this folder will contain the whole routing solver.

[tsplib95]: http://www.iwr.uni-heidelberg.de/groups/comopt/software/TSPLIB95/DOC.PS
[tsptw]: https://homepages.dcc.ufmg.br/~rfsilva/tsptw/
[solomon]: https://www.sintef.no/projectweb/top/vrptw/solomon-benchmark/
[homberger]: https://www.sintef.no/projectweb/top/vrptw/homberger-benchmark/
[pdtsp]: https://web.archive.org/web/20080318001744/http://www.diku.dk/~sropke/
[nearplib]: https://www.sintef.no/projectweb/top/nearp/
[carplib]: https://www.uv.es/belengue/carp.html
[lilim]: https://www.sintef.no/projectweb/top/pdptw/li-lim-benchmark/
[canad]: http://groups.di.unipi.it/optimize/Data/MMCF.html
