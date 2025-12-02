import type { CpSatApi } from '../lib';

declare global {
  interface Window {
    CpSat?: CpSatApi;
  }
}

export {};
