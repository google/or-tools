import { defineConfig } from 'vite';
import path from 'node:path';
import topLevelAwait from "vite-plugin-top-level-await";
import dts from 'vite-plugin-dts';

const rootDir = __dirname;
const libRoot = path.resolve(__dirname, 'javascript/lib');
const wasmBuildDir = path.resolve(__dirname, 'build/javascript/wasm');
const outDir = path.resolve(__dirname, 'build/javascript/lib');


// vite.lib.config.ts
const patchEmscriptenWasmPlugin = () => ({
  name: 'patch-emscripten-no-inline',
  transform(code, id) {
    if (id.includes('cp_sat_runtime') && id.endsWith('.js')) {
      let modifiedCode = code;

      // 1. WASM Fix: Keep WASM external
      // Matches: new URL('file.wasm', ...)
      if (modifiedCode.includes('.wasm')) {
        console.log('[patch-emscripten] PATCHING .wasm IN', path.basename(id), "with ?no-inline");
        modifiedCode = modifiedCode.replace(
          /new\s+URL\s*\(\s*['"]([^'"]+\.wasm)['"]\s*,/g,
          (match, filename) => `new URL("${filename}?no-inline",`
        );
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
  }
});

export default defineConfig({
  root: rootDir,
  assetsInclude: ['**/*.d.ts'],
  resolve: {
    alias: {
      '@internal-wasm': wasmBuildDir
    }
  },
  plugins: [
    topLevelAwait(),
    patchEmscriptenWasmPlugin(),
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
        sourcemap: true
      }
    }
  },
  build: {
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
        assetFileNames: 'assets/[name]-[hash][extname]',
        chunkFileNames: 'chunks/[name]-[hash].js',
      },
      external: ['module', 'worker_threads', 'fs', 'path', 'url'],
    },
  },
});
