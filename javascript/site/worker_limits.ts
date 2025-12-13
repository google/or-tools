const PTHREAD_POOL_SIZE = 8;

export const MAX_PTHREAD_POOL_SIZE = PTHREAD_POOL_SIZE;

export function getMaxWorkerCount(): number {
  const hardware =
    typeof navigator !== 'undefined' && typeof navigator.hardwareConcurrency === 'number'
      ? navigator.hardwareConcurrency
      : PTHREAD_POOL_SIZE;
  return Math.max(1, Math.min(PTHREAD_POOL_SIZE, hardware));
}
