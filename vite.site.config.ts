import { defineConfig, searchForWorkspaceRoot } from 'vite';
import path from 'node:path';

const siteRoot = path.resolve(__dirname, 'javascript/site');
const distDir = path.resolve(__dirname, 'build/javascript/site');

export default defineConfig({
  root: siteRoot,
  base: './',
  publicDir: path.join(siteRoot, 'public'),
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
  build: {
    outDir: distDir,
    emptyOutDir: true,
    assetsInlineLimit: 0,
    rollupOptions: {
      input: {
        index: path.resolve(siteRoot, 'index.html'),
        magic_square: path.resolve(siteRoot, 'magic_square.html'),
        model_playground: path.resolve(siteRoot, 'model_playground.html'),
        schema_viewer: path.resolve(siteRoot, 'schema_viewer.html'),
        sports_scheduling: path.resolve(siteRoot, 'sports_scheduling.html'),
        steel_mill_slab: path.resolve(siteRoot, 'steel_mill_slab.html'),
      },
    },
  },
});
