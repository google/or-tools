import { defineConfig } from 'vite';
import path from 'node:path';
import wasm from "vite-plugin-wasm";
import topLevelAwait from "vite-plugin-top-level-await";

const libRoot = path.resolve(__dirname, 'javascript/lib');
const outDir = path.resolve(__dirname, 'build/javascript/lib');

export default defineConfig({
  root: libRoot,
  plugins: [
    wasm(),
    topLevelAwait()
  ],
  worker: {
    format: 'es', // Force worker to be an ES module (allows TLA)
    plugins: () => [
      wasm(),
      topLevelAwait()
    ],
    rollupOptions: {
      output: {
        sourcemap: true // Explicitly enable for worker
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
    emptyOutDir: false,
    sourcemap: true,
    minify: 'esbuild',
    rollupOptions: {
      output: {
        entryFileNames: '[name].js',
        chunkFileNames: '[name].js',
        assetFileNames: '[name].[ext]',
      },
      // Silence the "Node-only" warnings since we are in browser
      external: ['module', 'worker_threads', 'fs', 'path', 'url'],
    },
  },
});
