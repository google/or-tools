declare module '@internal-wasm/cp_sat_runtime_asyncify.js' {
  import type { MainModule } from '@internal-wasm/cp_sat_runtime.js';

  const createModule: (moduleOverrides?: Record<string, unknown>) => Promise<MainModule>;
  export default createModule;
  export type { MainModule };
}
