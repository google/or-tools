import createModule from '@internal-wasm/cp_sat_runtime.js';
import type { MainModule } from '@internal-wasm/cp_sat_runtime.js';

export async function loadCpSat(): Promise<MainModule> {
  const module = await createModule({});
  return module;
}
