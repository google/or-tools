import { nodeResolve } from '@rollup/plugin-node-resolve';
import OMT from '@surma/rollup-plugin-off-main-thread';
import { importMetaAssets } from '@web/rollup-plugin-import-meta-assets';

function moduleRelativeFileUrls() {
  return {
    name: 'module-relative-file-urls',
    resolveFileUrl({ fileName }) {
      return `new URL(${JSON.stringify(fileName)}, import.meta.url).href`;
    },
  };
}

export default {
  input: 'src/main.js',
  output: {
    dir: 'dist',
    format: 'es',
  },
  plugins: [
    nodeResolve({
      browser: true,
    }),
    moduleRelativeFileUrls(),
    OMT(),
    importMetaAssets(),
  ],
};
