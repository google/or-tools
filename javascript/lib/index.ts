/**
 * Library entry exposing the public CP-SAT API for bundlers.
 * Re-export the module implementation that is used in the site demos.
 */
export { CpSat } from './cp_sat_api.js';
export { default } from './cp_sat_api.js';
export type { CpSatApi, CpSatModelInstance, CpSatSolveResult } from './cp_sat_api.js';
export type { SatParameters } from './generated/sat_parameters.js';
export type { WorkerRequest, WorkerResponse } from './cpsat_worker_types.js';

import cpSatRuntimeTypesUrl from '@internal-wasm/cp_sat_runtime.d.ts?url';
export const cpSatRuntimeTypesHref = cpSatRuntimeTypesUrl;
