import { readFile, writeFile } from 'node:fs/promises';
import path from 'node:path';

const repoRoot = path.resolve(import.meta.dirname, '..');
const wasmDir = path.join(repoRoot, 'build/javascript/wasm');
const runtimeFiles = ['cp_sat_runtime.js', 'cp_sat_runtime_asyncify.js'];

const patches = [
  {
    name: 'deno-runtime-detection',
    prelude: `
var __orToolsWasmDeno = typeof Deno !== "undefined";
if (__orToolsWasmDeno) {
  globalThis.window ??= globalThis;
  globalThis.self ??= globalThis;
  globalThis.WorkerGlobalScope ??= class WorkerGlobalScope {};
}
`,
    replacements: [
      [
        'var currentNodeVersion=typeof process!=="undefined"&&process.versions?.node?humanReadableVersionToPacked(process.versions.node):TARGET_NOT_SUPPORTED;',
        'var currentNodeVersion=typeof Deno==="undefined"&&typeof process!=="undefined"&&process.versions?.node?humanReadableVersionToPacked(process.versions.node):TARGET_NOT_SUPPORTED;',
      ],
      [
        'var ENVIRONMENT_IS_WEB=!!globalThis.window;',
        'var ENVIRONMENT_IS_WEB=!!globalThis.window||__orToolsWasmDeno;',
      ],
      [
        'var ENVIRONMENT_IS_NODE=globalThis.process?.versions?.node&&globalThis.process?.type!="renderer";',
        'var ENVIRONMENT_IS_NODE=!__orToolsWasmDeno&&globalThis.process?.versions?.node&&globalThis.process?.type!="renderer";',
      ],
    ],
  },
];

function applyPatch(code, patch) {
  let patched = code;
  const preludeMarker = patch.prelude.trim().split('\n')[0];
  if (!patched.includes(preludeMarker)) {
    patched = `${patch.prelude}\n${patched}`;
  }
  for (const [from, to] of patch.replacements) {
    if (!patched.includes(from) && !patched.includes(to)) {
      throw new Error(`Could not find Emscripten snippet for ${patch.name}: ${from}`);
    }
    patched = patched.replace(from, to);
  }
  return patched;
}

for (const fileName of runtimeFiles) {
  const filePath = path.join(wasmDir, fileName);
  const original = await readFile(filePath, 'utf8');
  const patched = patches.reduce((code, patch) => applyPatch(code, patch), original);
  if (patched !== original) {
    await writeFile(filePath, patched);
    console.log(`Patched ${fileName}: ${patches.map((patch) => patch.name).join(', ')}`);
  }
}
