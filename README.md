# or-tools-wasm

Unofficial JavaScript and WebAssembly bindings for the OR-Tools CP-SAT solver.

[GitHub](https://github.com/Axelwickm/or-tools-wasm)
[npm](https://www.npmjs.com/package/or-tools-wasm)
[Try Online](https://axelwickman.com/ortools-cpsat-wasm)

[![Package](https://github.com/Axelwickm/or-tools-wasm/actions/workflows/package.yml/badge.svg)](https://github.com/Axelwickm/or-tools-wasm/actions/workflows/package.yml)

Currently supported solvers: CP-SAT.

Verified with:

- Vite in browser contexts

This package builds a browser-oriented CP-SAT runtime from Google OR-Tools and
wraps it with a TypeScript API, worker bridge, generated SAT parameter types,
and Vite-powered demos.

The upstream solver source is vendored from
[google/or-tools](https://github.com/google/or-tools).

Browser deployments that use threaded solving must serve the page with cross-origin
isolation headers:

`Cross-Origin-Opener-Policy: same-origin`

`Cross-Origin-Embedder-Policy: require-corp`

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
npm install or-tools-wasm
```

Then import it normally:

```ts
import { CpSat } from 'or-tools-wasm';
```

This flow is verified with Vite. The worker script and WebAssembly files are
emitted automatically from the package import, with no manual copying into
`public/` or `static/` required.

For Vite dev mode, exclude `or-tools-wasm` from dependency optimization so Vite
handles the package's worker and WebAssembly URLs as source assets, and include
`protobufjs` so Vite prebundles its CommonJS entry:

```ts
// vite.config.ts
import { defineConfig } from 'vite';

export default defineConfig({
  optimizeDeps: {
    include: ['protobufjs'],
    exclude: ['or-tools-wasm'],
  },
  worker: {
    format: 'es',
  },
  server: {
    headers: {
      'Cross-Origin-Opener-Policy': 'same-origin',
      'Cross-Origin-Embedder-Policy': 'require-corp',
    },
  },
  preview: {
    headers: {
      'Cross-Origin-Opener-Policy': 'same-origin',
      'Cross-Origin-Embedder-Policy': 'require-corp',
    },
  },
});
```

Other modern bundlers may also work if they support module workers and WebAssembly
asset emission, but that is not yet officially verified.

If you use the worker bridge, the app must still be served with cross-origin
isolation headers:

```http
Cross-Origin-Opener-Policy: same-origin
Cross-Origin-Embedder-Policy: require-corp
```

## Usage

```ts
const model = {
  name: 'choose_one',
  variables: [
    { name: 'x', domain: [0, 1] },
    { name: 'y', domain: [0, 1] },
  ],
  constraints: [
    {
      name: 'exactly_one',
      linear: {
        vars: [0, 1],
        coeffs: [1, 1],
        domain: [1, 1],
      },
    },
  ],
  objective: {
    vars: [0, 1],
    coeffs: [1, 2],
  },
};

const modelBytes = await CpSat.createModel(model);
const validation = await CpSat.validate(modelBytes);

if (!validation.ok) {
  throw new Error(validation.message);
}

const result = await CpSat.solve(modelBytes, {
  numSearchWorkers: 1,
});

console.log(result.response);
```

## Build

```sh
npm install
npm run build
npm run preview
```

`npm run build` runs the full pipeline: Emscripten/CMake builds the low-level
CP-SAT WebAssembly runtime, Vite builds the package bundle, and Vite builds the
static demo site.

The Emscripten SDK is tracked as a pinned `emsdk` git submodule. The build
script initializes that submodule automatically if needed, so a normal clone can
run `npm run build` directly after `npm install`. If you prefer to fetch
submodules up front, clone with `--recurse-submodules` or run
`git submodule update --init --recursive`.

## Browser hosting requirements

This package uses a threaded WebAssembly runtime. Browser pages that load it
must be served with cross-origin isolation enabled:

```http
Cross-Origin-Opener-Policy: same-origin
Cross-Origin-Embedder-Policy: require-corp
```

Without these headers, browsers may block the `SharedArrayBuffer` APIs required
by Emscripten pthreads, and solving can fail during WebAssembly runtime or
worker startup.

## npm scripts

- `npm run build:wasm` rebuilds the `cp_sat_runtime` wasm/js bundle via emsdk + CMake.
- `npm run build:lib` regenerates SAT parameter types, type-checks with `tsc`, and builds the library bundle with `vite.lib.config.ts`.
- `npm run build:site` builds the demo site with `vite.site.config.ts`. The site imports `or-tools-wasm` directly, so Vite emits the worker/runtime/wasm assets from the package bundle automatically.
- `npm run dev` / `npm run start` builds the library and launches the Vite dev server for the demo site.
- `npm run build` runs `build:wasm`, `build:lib`, and `build:site`.
- `npm run preview` serves the already-built Vite site from `build/javascript/site`.
- `npm run clean` removes the entire `build/` tree.
- `npm run pack:lib` rebuilds the library bundle and writes an npm tarball into `build/javascript/lib`.

## Project layout

- `javascript/lib` contains the TypeScript package API and worker bridge.
- `javascript/site` contains the demo pages.
- `javascript/cp_sat_api.cc` contains the C++ binding layer compiled into WebAssembly.
- `scripts/embed_proto.cmake` embeds CP-SAT proto schemas into the runtime.
- `scripts/generate_sat_parameters_types.mjs` generates TypeScript SAT parameter definitions.
- `vite.lib.config.ts` builds the distributable JS package.
- `vite.site.config.ts` builds the demo site.

## Demos

- The Magic Square and Sports Scheduling pages let users pick a worker count; that value becomes `SatParameters.num_search_workers`, clamped to `min(navigator.hardwareConcurrency, 8)`.
- Higher worker counts can significantly increase startup time because the browser needs to initialize more threaded runtime workers before solving begins.
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
