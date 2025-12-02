import { defineConfig } from 'vite';
import path from 'node:path';

const siteRoot = path.resolve(__dirname, 'javascript/site');
const distDir = path.resolve(__dirname, 'build/javascript/site');
const libBuildDir = path.resolve(__dirname, 'build/javascript/lib');
const wasmSourceDir = path.resolve(__dirname, 'build/javascript/wasm');

export default defineConfig({
  root: siteRoot,
  base: './',
  publicDir: path.join(siteRoot, 'public'),
  worker: {
    format: 'es',
  },
  server: {
    fs: {
      allow: [siteRoot, wasmSourceDir, libBuildDir],
    },
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
  build: {
    outDir: distDir,
    emptyOutDir: true,
    rollupOptions: {
      input: {
        index: path.resolve(siteRoot, 'index.html'),
        magic_square: path.resolve(siteRoot, 'magic_square.html'),
        schema_viewer: path.resolve(siteRoot, 'schema_viewer.html'),
        sports_scheduling: path.resolve(siteRoot, 'sports_scheduling.html'),
        steel_mill_slab: path.resolve(siteRoot, 'steel_mill_slab.html'),
      },
    },
  },
});
