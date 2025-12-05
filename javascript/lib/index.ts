/**
 * Library entry exposing the public CP-SAT API for bundlers.
 * Re-export the module implementation that is used in the site demos.
 */
export { CpSat } from './cp_sat_api';
export { default } from './cp_sat_api';
export type { CpSatApi, CpSatSolveResult } from './cp_sat_api';
export type { WorkerRequest, WorkerResponse } from './cpsat_worker_types';
