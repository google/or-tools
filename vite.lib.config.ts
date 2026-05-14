import { defineConfig } from 'vite';
import type { Plugin } from 'vite';
import path from 'node:path';
import { readFileSync } from 'node:fs';
import topLevelAwait from 'vite-plugin-top-level-await';
import dts from 'vite-plugin-dts';

const rootDir = __dirname;
const libRoot = path.resolve(__dirname, 'javascript/lib');
const wasmBuildDir = path.resolve(__dirname, 'build/javascript/wasm');
const outDir = path.resolve(__dirname, 'build/javascript/lib');
const bundledLoaderPath = path.join(libRoot, 'cp_sat_module_loader.bundled.ts');

const unwrapDataUrlWorkers = (code: string) =>
  code.replace(
    /new\s+URL\s*\(\s*(["'])(data:text\/javascript;base64,[^"']+)\1\s*,\s*import\.meta\.url\s*\)/g,
    (_match: string, quote: string, dataUrl: string) => `${quote}${dataUrl}${quote}`
  );

const patchEmscriptenWasmPlugin = (): Plugin => ({
  name: 'patch-emscripten-no-inline',
  transform(code, id) {
    if (id.includes('cp_sat_runtime') && id.endsWith('.js')) {
      let modifiedCode = code;

      if (modifiedCode.includes('.wasm')) {
        modifiedCode = modifiedCode.replace(
          /new\s+URL\s*\(\s*['"]([^'"]+\.wasm)['"]\s*,/g,
          (_match: string, filename: string) => `new URL("${filename}?no-inline",`
        );
      }

      if (modifiedCode.includes('data:text/javascript;base64,')) {
        modifiedCode = unwrapDataUrlWorkers(modifiedCode);
      }

      if (modifiedCode.includes('new Worker')) {
        modifiedCode = modifiedCode.replace(
          /new\s+Worker\s*\(\s*(\/\*\s*@vite-ignore\s*\*\/\s*)?(new\s+URL\s*\(\s*["'][^"']+["']\s*,\s*import\.meta\.url\s*\))\s*,\s*(\/\*\s*@vite-ignore\s*\*\/\s*)?\{/g,
          (_match, urlIgnore, workerUrl, optionsIgnore) =>
            `new Worker(${urlIgnore ?? '/* @vite-ignore */ '}${workerUrl}, ${optionsIgnore ?? '/* @vite-ignore */ '}{`
        );
        modifiedCode = modifiedCode.replace(
          /new\s+Worker\s*\(\s*([A-Za-z_$][\w$]*(?:\.[A-Za-z_$][\w$]*)?)\s*,\s*(\/\*\s*@vite-ignore\s*\*\/\s*)?\{/g,
          (_match, workerUrl, existingIgnore) =>
            `new Worker(${workerUrl}, ${existingIgnore ?? '/* @vite-ignore */ '}{`
        );
      }

      return {
        code: modifiedCode,
        map: null
      };
    }
  },
  renderChunk(code) {
    if (code.includes('data:text/javascript;base64,')) {
      return {
        code: unwrapDataUrlWorkers(code),
        map: null,
      };
    }
  }
});

const emitWasmSourceMapsPlugin = (): Plugin => ({
  name: 'emit-wasm-source-maps',
  generateBundle() {
    for (const fileName of ['cp_sat_runtime.wasm.map', 'cp_sat_runtime_asyncify.wasm.map']) {
      this.emitFile({
        type: 'asset',
        fileName: `assets/${fileName}`,
        source: readFileSync(path.join(wasmBuildDir, fileName)),
      });
    }
  },
});

export default defineConfig({
  root: rootDir,
  base: './',
  assetsInclude: ['**/*.d.ts'],
  resolve: {
    alias: {
      './cp_sat_module_loader.js': bundledLoaderPath,
      '@internal-wasm': wasmBuildDir
    }
  },
  plugins: [
    topLevelAwait(),
    patchEmscriptenWasmPlugin(),
    emitWasmSourceMapsPlugin(),
    dts({
      tsconfigPath: path.resolve(__dirname, 'tsconfig.json'),
      rollupTypes: true,
      entryRoot: 'javascript/lib',
    }),
  ],
  worker: {
    format: 'es',
    plugins: () => [
      topLevelAwait(),
      patchEmscriptenWasmPlugin(),
    ],
    rollupOptions: {
      output: {
        assetFileNames: 'assets/[name][extname]',
      },
    },
  },
  build: {
    target: 'esnext',
    lib: {
      entry: path.join(libRoot, 'index.ts'),
      formats: ['es'],
      fileName: 'cp_sat_api',
    },
    outDir,
    emptyOutDir: true,
    assetsInlineLimit: 0,
    sourcemap: true,
    rollupOptions: {
      output: {
        assetFileNames: 'assets/[name][extname]',
        chunkFileNames: 'chunks/[name]-[hash].js',
      },
      external: ['module', 'worker_threads', 'fs', 'path', 'url'],
    },
  },
});
