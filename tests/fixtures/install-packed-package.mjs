import { readdir, stat } from 'node:fs/promises';
import { spawn } from 'node:child_process';
import path from 'node:path';
import process from 'node:process';

const fixtureName = process.argv[2];

if (!fixtureName) {
  console.error('Usage: node tests/fixtures/install-packed-package.mjs <fixture-name>');
  process.exit(1);
}

const repoRoot = path.resolve(import.meta.dirname, '../..');
const fixtureDir = path.join(repoRoot, 'tests/fixtures', fixtureName);
const packageDir = path.join(repoRoot, 'build/javascript/lib');

const entries = await readdir(packageDir).catch(() => {
  console.error(`No package directory found at ${packageDir}. Run npm run pack:lib first.`);
  process.exit(1);
});

const tarballs = await Promise.all(
  entries
    .filter((entry) => /^or-tools-wasm-.*\.tgz$/.test(entry))
    .map(async (entry) => {
      const tarballPath = path.join(packageDir, entry);
      return {
        path: tarballPath,
        mtimeMs: (await stat(tarballPath)).mtimeMs,
      };
    }),
);

tarballs.sort((a, b) => b.mtimeMs - a.mtimeMs);

if (!tarballs.length) {
  console.error(`No packed package found in ${packageDir}. Run npm run pack:lib first.`);
  process.exit(1);
}

const tarball = tarballs[0].path;
console.log(`Installing ${tarball} into ${fixtureDir}`);

const install = spawn('npm', ['install', '--force', '--no-audit', '--no-fund', tarball], {
  cwd: fixtureDir,
  stdio: 'inherit',
});

install.on('exit', (code) => {
  process.exit(code ?? 1);
});
