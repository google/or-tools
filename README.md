# ortools-cpsat-wasm

JavaScript and WebAssembly bindings for the OR-Tools CP-SAT solver.

This package builds a browser-oriented CP-SAT runtime from Google OR-Tools and
wraps it with a TypeScript API, worker bridge, generated SAT parameter types,
and Vite-powered demos.

The upstream solver source is vendored from
[google/or-tools](https://github.com/google/or-tools).

This repository currently vendors OR-Tools 9.14 pre-release sources
(`Version.txt` reports `OR_TOOLS_MAJOR=9`, `OR_TOOLS_MINOR=14`, and
`PRE_RELEASE` enabled).

## What is included

- A CP-SAT WebAssembly runtime built with Emscripten pthread support.
- A TypeScript API for solving, validating models, interrupting solves, and
  reading embedded proto schemas.
- A worker bridge for keeping browser UI threads responsive while solving.
- Generated TypeScript definitions for SAT parameters.
- Demo pages for Magic Square, Sports Scheduling, Steel Mill Slab, and schema
  inspection.

## Install

```sh
npm install ortools-cpsat-wasm
```

The published package exposes the library bundle under `build/javascript/lib`.

## Build

```sh
npm install
npm run build
npm run preview
```

`npm run build` runs the full pipeline: Emscripten/CMake builds the low-level
CP-SAT WebAssembly runtime, Vite builds the package bundle, and Vite builds the
static demo site.

## npm scripts

- `npm run build:wasm` rebuilds the `cp_sat_runtime` wasm/js bundle via emsdk + CMake.
- `npm run build:lib` regenerates SAT parameter types, type-checks with `tsc`, and builds the library bundle with `vite.lib.config.ts`.
- `npm run sync:lib-assets-to-site` clears `javascript/site/public/assets` and copies in the current library runtime assets from `build/javascript/lib/assets`.
- `npm run build:site` syncs the library assets, then builds the demo site with `vite.site.config.ts`.
- `npm run dev` / `npm run start` builds the library, syncs runtime assets into the demo site, and launches the Vite dev server.
- `npm run build` runs `build:wasm`, `build:lib`, and `build:site`.
- `npm run preview` serves the already-built Vite site from `build/javascript/site`.
- `npm run clean` removes the entire `build/` tree.
- `npm run pack:lib` rebuilds the library bundle and writes an npm tarball into `build/javascript/lib`.

## Project layout

- `javascript/lib` contains the TypeScript package API and worker bridge.
- `javascript/site` contains the demo pages.
- `javascript/site/public/assets` contains generated runtime assets copied from the latest library build.
- `javascript/cp_sat_api.cc` contains the C++ binding layer compiled into WebAssembly.
- `scripts/embed_proto.cmake` embeds CP-SAT proto schemas into the runtime.
- `scripts/generate_sat_parameters_types.mjs` generates TypeScript SAT parameter definitions.
- `vite.lib.config.ts` builds the distributable JS package.
- `vite.site.config.ts` builds the demo site.

## Demos

- The Magic Square and Sports Scheduling pages let users pick a worker count; that value becomes `SatParameters.num_search_workers`, clamped to `min(navigator.hardwareConcurrency, 8)`.
- Each demo exposes a "Use worker bridge" checkbox. When enabled, solves run through `cpsat_worker.ts`, which loads the CP-SAT runtime in a dedicated JavaScript worker and keeps the browser's main thread free for rendering, form input, progress UI, and cancellation.
- When the worker bridge is disabled, the solve runs directly on the main thread. The solver still works, but long solves can freeze the page until CP-SAT returns because the browser cannot repaint or process UI events while the synchronous WebAssembly call is running.
- The WebAssembly build caps the pthread pool at 8 and scales down on low-core devices.
- Schema Viewer imports the bundled `CpSat` API automatically; no extra script ordering is required.

## JSPI and Asyncify

The package ships two CP-SAT runtime builds:

- `cp_sat_runtime` uses Emscripten JSPI support (`-sJSPI=2`) for browsers that expose `WebAssembly.promising`.
- `cp_sat_runtime_asyncify` uses classic Asyncify stack rewriting for browsers without JSPI support.

`javascript/lib/cp_sat_module_loader.ts` chooses the runtime at startup. If
`WebAssembly.promising` is available, it imports the JSPI runtime, which is the
more modern and faster path. Otherwise it falls back to the Asyncify runtime.
Both paths expose the same TypeScript API, so application code does not need to
choose one manually. Current browser support for JSPI is tracked at
[caniuse.com/wf-wasm-jspi](https://caniuse.com/wf-wasm-jspi).

## Upstream OR-Tools

This repository vendors Google OR-Tools and adds a JavaScript/WebAssembly
packaging layer on top. OR-Tools is Google's open-source suite for solving
combinatorial optimization problems, including CP-SAT, linear programming,
routing, bin packing, and graph algorithms.

Upstream project:

- Source: [github.com/google/or-tools](https://github.com/google/or-tools)
- Documentation: [developers.google.com/optimization](https://developers.google.com/optimization/)
- License: Apache License 2.0

## License

This project is licensed under the Apache License 2.0. See [LICENSE](LICENSE).
