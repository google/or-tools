import type { MainModule } from '@internal-wasm/cp_sat_runtime.js';

type CpSatModuleFactory = (moduleOverrides?: Record<string, unknown>) => Promise<MainModule>;
type RuntimeFlavor = 'jspi' | 'asyncify';

let modulePromise: Promise<MainModule> | null = null;
let selectedFlavor: RuntimeFlavor | null = null;

function isJspiSupported(): boolean {
  const wasm = WebAssembly as typeof WebAssembly & { promising?: unknown };
  return typeof wasm !== 'undefined' && typeof wasm.promising === 'function';
}

function selectRuntimeFlavor(): RuntimeFlavor {
  if (selectedFlavor) {
    return selectedFlavor;
  }
  selectedFlavor = isJspiSupported() ? 'jspi' : 'asyncify';
  if (selectedFlavor === 'jspi') {
    console.log('🚀 JSPI is supported! Using ASYNCIFY=2 (High Performance).');
  } else {
    console.log('🐢 JSPI not found. Falling back to ASYNCIFY=1 (Stack Rewriting).');
  }
  return selectedFlavor;
}

async function loadFactory(): Promise<CpSatModuleFactory> {
  const flavor = selectRuntimeFlavor();
  if (flavor === 'jspi') {
    const { default: createModule } = await import('@internal-wasm/cp_sat_runtime.js');
    return createModule as CpSatModuleFactory;
  }
  const { default: createModule } = await import('@internal-wasm/cp_sat_runtime_asyncify.js');
  return createModule as CpSatModuleFactory;
}

export async function loadCpSat(): Promise<MainModule> {
  if (!modulePromise) {
    modulePromise = (async () => {
      const createModule = await loadFactory();
      return createModule({});
    })();
  }
  return modulePromise;
}
