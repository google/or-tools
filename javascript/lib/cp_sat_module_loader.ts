// import ModuleFactory from '@cp-sat-runtime';
// import type { MainModule } from '@cp-sat-runtime';

// Inside your loadCpSatModuleFactory wrapper
// Note: Adjust path to where your JS glue actually lives
import createModule from '@internal-wasm/cp_sat_runtime.js';

export async function loadCpSat() {
  const module = await createModule({
  });

  return module;
}

// 1. THE FIX: Import the Emscripten Glue JS as a RAW string.
// This creates a variable containing the code, rather than a file reference.
// Vite will inline this string into your bundle, but won't parse it for Worker imports.
// import runtimeGlueCode from '@internal-wasm/cp_sat_runtime.js?raw';

// Optional: If you want Vite to manage the WASM file location:
// import wasmAssetUrl from '@internal-wasm/cp_sat_runtime.wasm?url'; 

// type CpSatModuleFactory = (moduleArg?: Record<string, unknown>) => Promise<MainModule>;
//
// // Cache the Blob URL so we don't create a new Blob every time we spawn a solver
// let cachedWorkerUrl: string | null = null;

// function getWorkerBlobUrl(): string {
//   if (cachedWorkerUrl) return cachedWorkerUrl;
//
//   // 2. Create a Blob from the raw string
//   const blob = new Blob([runtimeGlueCode], { type: 'application/javascript' });
//
//   // 3. Create a stable URL for that Blob
//   cachedWorkerUrl = URL.createObjectURL(blob);
//
//   return cachedWorkerUrl;
// }
//
// export function loadCpSatModuleFactory(): Promise<CpSatModuleFactory> {
//   return Promise.resolve((args: Record<string, unknown> = {}) => {
//
//     const workerUrl = getWorkerBlobUrl();
//
//     return ModuleFactory({
//       // 4. INJECT THE BLOB URL
//       // Emscripten uses this property to determine what script to use 
//       // when spawning new Workers (or Pthreads).
//       // Because this is a variable, Vite ignores it.
//       mainScriptUrlOrBlob: workerUrl,
//
//       locateFile: (path: string, scriptDirectory: string) => {
//         // CRITICAL NOTE:
//         // Since the Worker is running from a "blob:..." URL, it has no "directory".
//         // Relative paths (like "./cp_sat.wasm") might fail depending on browser security.
//         // You usually want to return an absolute URL to the WASM file here.
//
//         if (path.endsWith('.wasm')) {
//           // If you imported the WASM with ?url above, return it here:
//           // return wasmAssetUrl;
//
//           // Otherwise, allow the user to override it via args, or fall back to default
//           return path;
//         }
//         return scriptDirectory + path;
//       },
//       ...args,
//     });
//   });
// }
