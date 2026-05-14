import { mkdir, readFile, rm, writeFile } from 'node:fs/promises';
import path from 'node:path';
import { build, transform } from 'esbuild';

const rootDir = path.resolve(import.meta.dirname, '..');
const sourceDir = path.join(rootDir, 'javascript/lib');
const outDir = path.join(rootDir, 'build/javascript/browser');

async function transpileSource(relativePath) {
  const sourcePath = path.join(sourceDir, relativePath);
  const outputPath = path.join(outDir, relativePath.replace(/\.ts$/, '.js'));
  const source = await readFile(sourcePath, 'utf8');
  const result = await transform(source, {
    loader: 'ts',
    format: 'esm',
    target: 'es2020',
    sourcemap: false,
  });

  await mkdir(path.dirname(outputPath), { recursive: true });
  await writeFile(outputPath, result.code);
}

const externalLoaderPlugin = {
  name: 'external-cp-sat-loader',
  setup(buildContext) {
    buildContext.onResolve({ filter: /^\.\/cp_sat_module_loader\.js$/ }, (args) => ({
      path: args.path,
      external: true,
    }));
  },
};

async function bundleEntry(entryPoint, outfile) {
  await build({
    entryPoints: [path.join(sourceDir, entryPoint)],
    outfile: path.join(outDir, outfile),
    bundle: true,
    format: 'esm',
    platform: 'browser',
    target: 'es2020',
    sourcemap: false,
    plugins: [externalLoaderPlugin],
  });
}

const browserLoader = `let modulePromise = null;
let selectedFlavor = null;

const cpSatRuntimeWasmUrl = new URL('../wasm/cp_sat_runtime.wasm', import.meta.url).href;
const cpSatRuntimeAsyncifyWasmUrl = new URL('../wasm/cp_sat_runtime_asyncify.wasm', import.meta.url).href;

function isJspiSupported() {
  const wasm = WebAssembly;
  return typeof wasm !== 'undefined' && typeof wasm.promising === 'function';
}

function selectRuntimeFlavor() {
  if (selectedFlavor) {
    return selectedFlavor;
  }
  selectedFlavor = isJspiSupported() ? 'jspi' : 'asyncify';
  if (selectedFlavor === 'jspi') {
    console.log('JSPI is supported. Using ASYNCIFY=2.');
  } else {
    console.log('JSPI not found. Falling back to ASYNCIFY=1.');
  }
  return selectedFlavor;
}

async function loadFactory() {
  const flavor = selectRuntimeFlavor();
  if (flavor === 'jspi') {
    const { default: createModule } = await import('../wasm/cp_sat_runtime.js');
    return createModule;
  }
  const { default: createModule } = await import('../wasm/cp_sat_runtime_asyncify.js');
  return createModule;
}

function locateCpSatRuntimeFile(fileName) {
  if (fileName === 'cp_sat_runtime.wasm') {
    return cpSatRuntimeWasmUrl;
  }
  if (fileName === 'cp_sat_runtime_asyncify.wasm') {
    return cpSatRuntimeAsyncifyWasmUrl;
  }
  return fileName;
}

export async function loadCpSat() {
  if (!modulePromise) {
    modulePromise = (async () => {
      const createModule = await loadFactory();
      return createModule({ locateFile: locateCpSatRuntimeFile });
    })();
  }
  return modulePromise;
}
`;

await rm(outDir, { recursive: true, force: true });
await mkdir(outDir, { recursive: true });
await bundleEntry('index.ts', 'index.js');
await bundleEntry('cpsat_worker.ts', 'cpsat_worker.js');
await transpileSource('cpsat_worker_types.ts');
await writeFile(path.join(outDir, 'cp_sat_module_loader.js'), browserLoader);
