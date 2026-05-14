import type { MainModule } from '@internal-wasm/cp_sat_runtime.js';
import cpSatRuntimeWasmUrl from '@internal-wasm/cp_sat_runtime.wasm?url&no-inline';
import cpSatRuntimeAsyncifyWasmUrl from '@internal-wasm/cp_sat_runtime_asyncify.wasm?url&no-inline';


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
    console.log('JSPI is supported. Using ASYNCIFY=2.');
  } else {
    console.log('JSPI not found. Falling back to ASYNCIFY=1.');
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

function locateCpSatRuntimeFile(fileName: string) {
  if (fileName === 'cp_sat_runtime.wasm') {
    return cpSatRuntimeWasmUrl;
  }
  if (fileName === 'cp_sat_runtime_asyncify.wasm') {
    return cpSatRuntimeAsyncifyWasmUrl;
  }
  return fileName;
}

export async function loadCpSat(): Promise<MainModule> {
  if (!modulePromise) {
    modulePromise = (async () => {
      const createModule = await loadFactory();
      return createModule({ locateFile: locateCpSatRuntimeFile });
    })();
  }
  return modulePromise;
}
